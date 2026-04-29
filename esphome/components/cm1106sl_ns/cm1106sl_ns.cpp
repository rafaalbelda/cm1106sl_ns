#include "cm1106sl_ns.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

void CM1106SLNSComponent::setup() {
  this->last_request_time_ = millis() - this->measurement_period_;
  this->measurement_start_time_ = 0;
  this->error_count_ = 0;
  this->last_status_ = 0;
  this->timeout_active_ = false;
  this->read_pending_ = false;
  
  ESP_LOGI(TAG, "CM1106SL-NS initialized on I2C register protocol at address 0x%02X", this->get_i2c_address());
  ESP_LOGI(TAG, "Read period: %us", this->measurement_period_ / 1000);

  if (!this->configure_measurement_mode_()) {
    this->set_error_(true);
    this->mark_failed();
    return;
  }

  if (!this->single_mode_)
    this->last_request_time_ = millis();
}

void CM1106SLNSComponent::set_internal_measurement_period(uint32_t period_ms) {
  uint32_t period_s = period_ms / 1000;
  if (period_s < 2)
    period_s = 2;
  if (period_s > 65534)
    period_s = 65534;
  if (period_s % 2 != 0)
    period_s++;
  this->internal_measurement_period_s_ = period_s;
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

bool CM1106SLNSComponent::configure_measurement_mode_() {
  const uint8_t mode = this->single_mode_ ? 0x01 : 0x00;
  if (!this->write_register_bytes_(REG_MEASUREMENT_MODE, &mode, 1)) {
    ESP_LOGW(TAG, "Failed to set %s measurement mode", this->single_mode_ ? "single" : "continuous");
    return false;
  }

  if (!this->single_mode_) {
    const uint8_t period_data[2] = {
        static_cast<uint8_t>(this->internal_measurement_period_s_ >> 8),
        static_cast<uint8_t>(this->internal_measurement_period_s_ & 0xFF),
    };
    if (!this->write_register_bytes_(REG_MEASUREMENT_PERIOD_HIGH, period_data, sizeof(period_data))) {
      ESP_LOGW(TAG, "Failed to set continuous measurement period");
      return false;
    }
  }

  ESP_LOGI(TAG, "%s measurement mode enabled", this->single_mode_ ? "Single" : "Continuous");
  return true;
}

bool CM1106SLNSComponent::start_single_measurement_() {
  const uint8_t command = 0x01;
  if (!this->write_register_bytes_(REG_START_SINGLE_MEASUREMENT, &command, 1)) {
    this->error_count_++;
    ESP_LOGW(TAG, "Failed to start single measurement (count: %u)", this->error_count_);
    this->set_error_(true);
    return false;
  }

  this->read_pending_ = true;
  this->measurement_start_time_ = millis();
  if (this->ready_sensor_ != nullptr)
    this->ready_sensor_->publish_state(false);
  if (this->debug_)
    ESP_LOGD(TAG, "Single measurement started");
  return true;
}

bool CM1106SLNSComponent::write_register_bytes_(uint8_t reg, const uint8_t *data, size_t len) {
  uint8_t buffer[8];
  if (len + 1 > sizeof(buffer)) {
    ESP_LOGW(TAG, "I2C write to 0x%02X is too long: %u bytes", reg, static_cast<unsigned>(len));
    return false;
  }

  buffer[0] = reg;
  memcpy(buffer + 1, data, len);
  auto err = this->write(buffer, len + 1);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C write to 0x%02X failed: %u", reg, err);
    return false;
  }
  return true;
}

bool CM1106SLNSComponent::read_register_bytes_(uint8_t reg, uint8_t *data, size_t len) {
  auto err = this->write(&reg, 1);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C register select 0x%02X failed: %u", reg, err);
    return false;
  }

  err = this->read(data, len);
  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C raw read from 0x%02X failed: %u", reg, err);
    return false;
  }

  return true;
}

bool CM1106SLNSComponent::read_measurement_() {
  uint8_t status = 0;
  uint8_t co2_data[2] = {0};

  if (!this->read_register_bytes_(REG_ERROR_STATUS, &status, 1)) {
    this->error_count_++;
    ESP_LOGW(TAG, "I2C status read error (count: %u)", this->error_count_);
    this->set_error_(true);
    return false;
  }

  if (!this->read_register_bytes_(REG_CO2_HIGH, co2_data, sizeof(co2_data))) {
    this->error_count_++;
    ESP_LOGW(TAG, "I2C read error (count: %u)", this->error_count_);
    this->set_error_(true);
    return false;
  }

  return this->publish_measurement_(((uint16_t) co2_data[0] << 8) | co2_data[1], status);
}

bool CM1106SLNSComponent::publish_measurement_(uint16_t co2, uint8_t status) {
  this->last_status_ = status;

  if (this->debug_)
    ESP_LOGD(TAG, "CO2: %u ppm, status: 0x%02X", co2, status);

  if (this->status_sensor_ != nullptr)
    this->status_sensor_->publish_state(status);

  if ((status & 0x21) != 0) {
    ESP_LOGW(TAG, "Sensor returned error status: 0x%02X", status);
    this->set_error_(true);
    return false;
  }

  // Datasheet range is 0-5000 ppm. Reject implausibly low startup values.
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

  if (!this->read_pending_) {
    if (now - this->last_request_time_ >= this->measurement_period_) {
      this->last_request_time_ = now;
      if (this->single_mode_)
        this->start_single_measurement_();
      else
        this->read_measurement_();
    }
    return;
  }

  if (now - this->measurement_start_time_ < SINGLE_MEASUREMENT_DELAY_MS)
    return;

  this->read_pending_ = false;
  this->read_measurement_();
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS (I2C Register Protocol):");
  LOG_SENSOR(" ", "CO2", this->co2_sensor_);
  LOG_SENSOR(" ", "Stability", this->stability_sensor_);
  LOG_BINARY_SENSOR(" ", "Ready", this->ready_sensor_);
  LOG_BINARY_SENSOR(" ", "Error", this->error_sensor_);
  LOG_SENSOR(" ", "IAQ Index", this->iaq_numeric_);
  LOG_SENSOR(" ", "Status", this->status_sensor_);
  ESP_LOGCONFIG(TAG, "Debug: %s", this->debug_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "Read period: %us", this->measurement_period_ / 1000);
  ESP_LOGCONFIG(TAG, "Measurement mode: %s", this->single_mode_ ? "single" : "continuous");
  if (!this->single_mode_)
    ESP_LOGCONFIG(TAG, "Internal measurement period: %us", this->internal_measurement_period_s_);
  ESP_LOGCONFIG(TAG, "Single measurement delay: %ums", SINGLE_MEASUREMENT_DELAY_MS);
}

}  // namespace cm1106sl_ns
}  // namespace esphome
