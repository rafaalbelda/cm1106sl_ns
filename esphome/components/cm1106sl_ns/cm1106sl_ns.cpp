#include "cm1106sl_ns.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

void CM1106SLNSComponent::setup() {
  this->last_request_time_ = millis() - this->measurement_period_;
  this->command_sent_time_ = 0;
  this->error_count_ = 0;
  this->last_status_ = 0;
  this->timeout_active_ = false;
  this->read_pending_ = false;
  
  ESP_LOGI(TAG, "CM1106SL-NS initialized with %s I2C protocol at address 0x%02X",
           this->command_protocol_ ? "command" : "register", this->get_i2c_address());
  ESP_LOGI(TAG, "Read period: %us", this->measurement_period_ / 1000);

  if (!this->command_protocol_) {
    if (!this->write_byte(REG_MEASUREMENT_MODE, 0x01)) {
      ESP_LOGW(TAG, "Failed to enable single measurement mode");
      this->set_error_(true);
      return;
    }
    ESP_LOGI(TAG, "Single measurement mode enabled");
  } else if (this->get_i2c_address() != CM1106_COMMAND_I2C_ADDRESS) {
    ESP_LOGW(TAG, "Command protocol usually uses address 0x%02X", CM1106_COMMAND_I2C_ADDRESS);
  }
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

uint8_t CM1106SLNSComponent::checksum_(const uint8_t *data, size_t len) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < len; i++)
    checksum -= data[i];
  return checksum;
}

bool CM1106SLNSComponent::request_measurement_() {
  const uint8_t command = CM1106_CMD_MEASURE_RESULT;
  auto err = this->write(&command, 1);
  if (err != i2c::ERROR_OK) {
    this->error_count_++;
    ESP_LOGW(TAG, "Failed to request measurement result (I2C error %u, count: %u)", err, this->error_count_);
    this->set_error_(true);
    return false;
  }

  this->read_pending_ = true;
  this->command_sent_time_ = millis();
  if (this->ready_sensor_ != nullptr)
    this->ready_sensor_->publish_state(false);
  if (this->debug_)
    ESP_LOGD(TAG, "Measurement result command sent");
  return true;
}

bool CM1106SLNSComponent::start_single_measurement_() {
  if (!this->write_byte(REG_START_SINGLE_MEASUREMENT, 0x01)) {
    this->error_count_++;
    ESP_LOGW(TAG, "Failed to start single measurement (count: %u)", this->error_count_);
    this->set_error_(true);
    return false;
  }

  this->read_pending_ = true;
  this->command_sent_time_ = millis();
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
  uint8_t buffer[5] = {0};
  auto err = this->read(buffer, sizeof(buffer));
  if (err != i2c::ERROR_OK) {
    this->error_count_++;
    ESP_LOGW(TAG, "I2C read error %u (count: %u)", err, this->error_count_);
    this->set_error_(true);
    return false;
  }

  if (buffer[0] != CM1106_CMD_MEASURE_RESULT) {
    this->error_count_++;
    ESP_LOGW(TAG, "Unexpected frame header: 0x%02X", buffer[0]);
    this->set_error_(true);
    return false;
  }

  const uint8_t checksum = CM1106SLNSComponent::checksum_(buffer, sizeof(buffer) - 1);
  if (buffer[4] != checksum) {
    this->error_count_++;
    ESP_LOGW(TAG, "Checksum mismatch: 0x%02X != 0x%02X", buffer[4], checksum);
    this->set_error_(true);
    return false;
  }

  return this->publish_measurement_(((uint16_t) buffer[1] << 8) | buffer[2], buffer[3]);
}

bool CM1106SLNSComponent::read_register_measurement_() {
  uint8_t co2_data[2] = {0};

  if (!this->read_register_bytes_(REG_CO2_HIGH, co2_data, sizeof(co2_data))) {
    this->error_count_++;
    ESP_LOGW(TAG, "I2C read error (count: %u)", this->error_count_);
    this->set_error_(true);
    return false;
  }

  return this->publish_measurement_(((uint16_t) co2_data[0] << 8) | co2_data[1], CM1106_STATUS_NORMAL_OPERATION);
}

bool CM1106SLNSComponent::publish_measurement_(uint16_t co2, uint8_t status) {
  this->last_status_ = status;

  if (this->debug_)
    ESP_LOGD(TAG, "CO2: %u ppm, status: 0x%02X", co2, status);

  if (this->status_sensor_ != nullptr)
    this->status_sensor_->publish_state(status);

  if (status != CM1106_STATUS_NORMAL_OPERATION && status != CM1106_STATUS_PREHEATING &&
      status != CM1106_STATUS_NON_CALIBRATED) {
    ESP_LOGW(TAG, "Sensor returned abnormal status: 0x%02X", status);
    this->set_error_(true);
    return false;
  }

  if (status == CM1106_STATUS_PREHEATING) {
    this->error_count_ = 0;
    this->set_error_(false);
    if (this->ready_sensor_ != nullptr)
      this->ready_sensor_->publish_state(false);
    if (this->debug_)
      ESP_LOGD(TAG, "Sensor is preheating, CO2 value skipped");
    return true;
  }

  // Datasheet range is 0-5000 ppm. Reject implausibly low non-warmup values.
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
      if (this->command_protocol_)
        this->request_measurement_();
      else
        this->start_single_measurement_();
    }
    return;
  }

  const uint32_t delay_ms = this->command_protocol_ ? CM1106_DELAY_FOR_ACK_MS : SINGLE_MEASUREMENT_DELAY_MS;
  if (now - this->command_sent_time_ < delay_ms)
    return;

  this->read_pending_ = false;
  if (this->command_protocol_)
    this->read_measurement_();
  else
    this->read_register_measurement_();
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS (%s I2C Protocol):", this->command_protocol_ ? "Command" : "Register");
  LOG_SENSOR(" ", "CO2", this->co2_sensor_);
  ESP_LOGCONFIG(TAG, "Stability", this->stability_sensor_);
  ESP_LOGCONFIG(TAG, "Ready", this->ready_sensor_);
  ESP_LOGCONFIG(TAG, "Error", this->error_sensor_);
  ESP_LOGCONFIG(TAG, "IAQ Index", this->iaq_numeric_);
  ESP_LOGCONFIG(TAG, "Status", this->status_sensor_);
  ESP_LOGCONFIG(TAG, "Debug: %s", this->debug_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "Read period: %us", this->measurement_period_ / 1000);
  ESP_LOGCONFIG(TAG, "Read delay: %ums", this->command_protocol_ ? CM1106_DELAY_FOR_ACK_MS : SINGLE_MEASUREMENT_DELAY_MS);


  this->setup();
}

}  // namespace cm1106sl_ns
}  // namespace esphome
