#include "cm1106sl_ns.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

void CM1106SLNSComponent::setup() {
  this->last_frame_time_ = millis();
  this->error_count_ = 0;
  this->timeout_active_ = false;
  
  this->set_i2c_address(0x34);
  
  ESP_LOGI(TAG, "CM1106SL-NS initialized on I2C at address 0x34");
  
  // Set measurement mode to continuous (0x00 = Mode B)
  // I2C command: 0x68 [0x95] [0x00]
  uint8_t mode_cmd[2] = {REG_MEASUREMENT_MODE, 0x00};
  if (!this->write(mode_cmd, 2)) {
    ESP_LOGW(TAG, "Failed to enable continuous measurement mode");
    return;
  }
  
  ESP_LOGI(TAG, "Continuous measurement mode enabled, period: %us", this->measurement_period_ / 1000);
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

void CM1106SLNSComponent::loop() {
  if (millis() - this->last_frame_time_ > this->measurement_period_) {
    uint8_t co2_data[2];
    
    // In continuous mode, the sensor automatically updates registers 0x06/0x07
    // Simply read CO2 concentration from registers
    if (!this->read_bytes(REG_CO2_HIGH, co2_data, 2)) {
      this->error_count_++;
      if (!this->timeout_active_) {
        ESP_LOGW(TAG, "I2C read error (count: %u)", this->error_count_);
        if (this->error_sensor_ != nullptr)
          this->error_sensor_->publish_state(true);
        this->timeout_active_ = true;
      }
      return;
    }
    
    this->last_frame_time_ = millis();
    this->error_count_ = 0;
    
    if (this->timeout_active_) {
      this->timeout_active_ = false;
      if (this->error_sensor_ != nullptr)
        this->error_sensor_->publish_state(false);
    }
    
    uint16_t co2 = ((uint16_t)co2_data[0] << 8) | co2_data[1];
    
    if (this->debug_)
      ESP_LOGD(TAG, "CO2: %u ppm (0x%02X 0x%02X)", co2, co2_data[0], co2_data[1]);
    
    // Validate CO2 range (datasheet: 300-5000ppm)
    if (co2 < 300 || co2 > 5000) {
      ESP_LOGW(TAG, "Out of range: %u ppm", co2);
      return;
    }
    
    // Update stability counter
    if (std::abs((int)co2 - (int)this->last_valid_co2_) < 20)
      this->stability_counter_ = std::min<uint8_t>(100, this->stability_counter_ + 1);
    else
      this->stability_counter_ = (this->stability_counter_ > 2) ? this->stability_counter_ - 2 : 0;
    
    this->last_valid_co2_ = co2;
    
    // Publish readings
    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(co2);
    
    if (this->stability_sensor_ != nullptr)
      this->stability_sensor_->publish_state(this->stability_counter_);
    
    if (this->ready_sensor_ != nullptr)
      this->ready_sensor_->publish_state(true);
    
    this->publish_iaq_(co2);
  }
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS (I2C Continuous Mode):");
  LOG_SENSOR(" ", "CO2", this->co2_sensor_);
  LOG_SENSOR(" ", "Stability", this->stability_sensor_);
  LOG_BINARY_SENSOR(" ", "Ready", this->ready_sensor_);
  LOG_BINARY_SENSOR(" ", "Error", this->error_sensor_);
  LOG_SENSOR(" ", "IAQ Index", this->iaq_numeric_);
  ESP_LOGCONFIG(TAG, "Debug: %s", this->debug_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "Measurement period: %us", this->measurement_period_ / 1000);
}

}  // namespace cm1106sl_ns
}  // namespace esphome
