#include "cm1106sl_ns.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

void CM1106SLNSComponent::setup() {
  this->last_frame_time_ = millis() - this->measurement_period_;
  this->measurement_start_time_ = 0;
  this->next_configure_time_ = millis() + INITIAL_CONFIGURE_DELAY_MS;
  this->error_count_ = 0;
  this->timeout_active_ = false;
  this->measurement_pending_ = false;
  this->configured_ = false;
  
  ESP_LOGI(TAG, "CM1106SL-NS initialized on I2C at address 0x%02X", this->get_i2c_address());
  ESP_LOGI(TAG, "Delaying first CM1106SL-NS configuration by %us", INITIAL_CONFIGURE_DELAY_MS / 1000);
}

void CM1106SLNSComponent::publish_iaq_(uint16_t co2) {
  int iaq = 1;  // Default to excellent
  
  if (co2 < 600) {
    iaq = 1;  // Excellent
  } else if (co2 < 800) {
    iaq = 2;  // Good
  } else if (co2 < 1000) {
    iaq = 3;  // Acceptable
  } else if (co2 < 1500) {
    iaq = 4;  // Poor
  } else {
    iaq = 5;  // Very poor
  }
  
  if (this->iaq_numeric_ != nullptr)
    this->iaq_numeric_->publish_state(iaq);
}

void CM1106SLNSComponent::set_error_(bool error) {
  if (this->error_sensor_ != nullptr)
    this->error_sensor_->publish_state(error);
  this->timeout_active_ = error;
}

bool CM1106SLNSComponent::configure_sensor_() {
  // Set measurement mode to single (0x01 = Mode A).
  if (!this->write_byte(REG_MEASUREMENT_MODE, 0x01)) {
    this->configured_ = false;
    this->measurement_pending_ = false;
    this->next_configure_time_ = millis() + CONFIGURE_RETRY_INTERVAL_MS;
    this->status_set_warning();
    this->set_error_(true);
    ESP_LOGW(TAG, "CM1106SL-NS not responding at 0x%02X; retrying configuration in %us", this->get_i2c_address(),
             CONFIGURE_RETRY_INTERVAL_MS / 1000);
    return false;
  }

  this->configured_ = true;
  this->error_count_ = 0;
  this->status_clear_warning();
  this->set_error_(false);
  ESP_LOGI(TAG, "Single measurement mode enabled, read period: %us", this->measurement_period_ / 1000);
  return true;
}

bool CM1106SLNSComponent::start_single_measurement_() {
  if (!this->write_byte(REG_START_SINGLE_MEASUREMENT, 0x01)) {
    this->error_count_++;
    ESP_LOGW(TAG, "Failed to start single measurement (count: %u)", this->error_count_);
    this->configured_ = false;
    this->next_configure_time_ = millis() + CONFIGURE_RETRY_INTERVAL_MS;
    this->set_error_(true);
    this->status_set_warning();
    return false;
  }

  this->measurement_pending_ = true;
  this->measurement_start_time_ = millis();
  if (this->ready_sensor_ != nullptr)
    this->ready_sensor_->publish_state(false);
  if (this->debug_)
    ESP_LOGD(TAG, "Single measurement started");
  return true;
}

bool CM1106SLNSComponent::read_register_bytes_(uint8_t reg, uint8_t *data, size_t len) {
  auto err = this->write(&reg, 1);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C register select 0x%02X failed: %u", reg, err);
    this->configured_ = false;
    this->next_configure_time_ = millis() + CONFIGURE_RETRY_INTERVAL_MS;
    this->status_set_warning();
    return false;
  }

  err = this->read(data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C raw read from 0x%02X failed: %u", reg, err);
    this->configured_ = false;
    this->next_configure_time_ = millis() + CONFIGURE_RETRY_INTERVAL_MS;
    this->status_set_warning();
    return false;
  }

  return true;
}

bool CM1106SLNSComponent::read_measurement_() {
  uint8_t co2_data[2];

  if (!this->read_register_bytes_(REG_CO2_HIGH, co2_data, sizeof(co2_data))) {
    this->error_count_++;
    ESP_LOGW(TAG, "I2C read error (count: %u)", this->error_count_);
    this->set_error_(true);
    return false;
  }

  uint16_t co2 = ((uint16_t)co2_data[0] << 8) | co2_data[1];

  if (this->debug_)
    ESP_LOGD(TAG, "CO2: %u ppm (0x%02X 0x%02X)", co2, co2_data[0], co2_data[1]);

  // Validate CO2 range (datasheet: 0-5000ppm; reject implausibly low startup values).
  if (co2 < 300 || co2 > 5000) {
    ESP_LOGW(TAG, "Out of range: %u ppm", co2);
    this->set_error_(true);
    return false;
  }

  this->error_count_ = 0;
  this->set_error_(false);

  if (std::abs((int)co2 - (int)this->last_valid_co2_) < 20)
    this->stability_counter_ = std::min<uint8_t>(100, this->stability_counter_ + 1);
  else
    this->stability_counter_ = (this->stability_counter_ > 2) ? this->stability_counter_ - 2 : 0;

  this->last_valid_co2_ = co2;

  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(co2);

  if (this->stability_sensor_ != nullptr)
    this->stability_sensor_->publish_state(this->stability_counter_);

  if (this->ready_sensor_ != nullptr)
    this->ready_sensor_->publish_state(true);

  this->publish_iaq_(co2);
  return true;
}

void CM1106SLNSComponent::loop() {
  const uint32_t now = millis();

  if (!this->configured_) {
    if ((int32_t) (now - this->next_configure_time_) < 0)
      return;
    this->configure_sensor_();
    return;
  }

  if (!this->measurement_pending_) {
    if (now - this->last_frame_time_ >= this->measurement_period_) {
      if (!this->start_single_measurement_())
        this->last_frame_time_ = now;
    }
    return;
  }

  if (now - this->measurement_start_time_ < SINGLE_MEASUREMENT_DELAY_MS)
    return;

  this->measurement_pending_ = false;
  this->last_frame_time_ = now;
  this->read_measurement_();
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS (I2C Single Measurement Mode):");
  LOG_SENSOR(" ", "CO2", this->co2_sensor_);
  LOG_SENSOR(" ", "Stability", this->stability_sensor_);
  LOG_BINARY_SENSOR(" ", "Ready", this->ready_sensor_);
  LOG_BINARY_SENSOR(" ", "Error", this->error_sensor_);
  LOG_SENSOR(" ", "IAQ Index", this->iaq_numeric_);
  ESP_LOGCONFIG(TAG, "Debug: %s", this->debug_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "Read period: %us", this->measurement_period_ / 1000);
  ESP_LOGCONFIG(TAG, "Single measurement delay: %ums", SINGLE_MEASUREMENT_DELAY_MS);
}

}  // namespace cm1106sl_ns
}  // namespace esphome
