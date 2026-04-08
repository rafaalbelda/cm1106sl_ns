#include "cm1106sl_ns.h"
#include "esphome/core/log.h"
#include <cstring>
#include <cmath>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

void CM1106SLNSComponent::setup() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS sensor setup");
  ESP_LOGD(TAG, "CM1106SL-NS sensor setup");
  this->last_frame_time_ = millis();
  
  // Send configuration command with slight delay to ensure sensor is ready
  delay(100);
  this->config_cmd_time_ = millis();
  this->send_config_command_();
}

std::string CM1106SLNSComponent::interpret_status_(uint8_t df3, uint8_t df4) {
  if (df3 == 0x08)
    return "Warming up";
  if (df3 == 0x00 && df4 == 0x00)
    return "Normal";
  if (df3 == 0x01)
    return "Sensor error";
  if (df3 == 0x02)
    return "Calibration required";
  return "Unknown";
}

bool CM1106SLNSComponent::validate_checksum_(const uint8_t *buffer, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len - 1; i++) {
    sum += buffer[i];
  }
  uint8_t checksum = (~sum) + 1;
  return checksum == buffer[len - 1];
}

uint8_t CM1106SLNSComponent::calculate_checksum_(const uint8_t *buffer, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += buffer[i];
  }
  return (~sum) + 1;
}

void CM1106SLNSComponent::publish_iaq_(uint16_t co2) {
  int iaq = 0;
  std::string label;

  if (co2 < 600) {
    iaq = 1;
    label = "Excelente";
  } else if (co2 < 800) {
    iaq = 2;
    label = "Buena";
  } else if (co2 < 1000) {
    iaq = 3;
    label = "Aceptable";
  } else if (co2 < 1500) {
    iaq = 4;
    label = "Mala";
  } else {
    iaq = 5;
    label = "Muy mala";
  }

  if (this->iaq_numeric_ != nullptr)
    this->iaq_numeric_->publish_state(iaq);
  // if (this->iaq_text_ != nullptr)
  //   this->iaq_text_->publish_state(label);
}

void CM1106SLNSComponent::soft_reset_() {
  ESP_LOGW(TAG, "Soft reset del sensor");
  const uint8_t reset_cmd[5] = {0x11, 0x03, 0x02, 0x00, 0xED};
  this->write_array(reset_cmd, 5);
}

void CM1106SLNSComponent::loop() {
  uint8_t buffer[8];

  // Handle config response if waiting
  if (this->awaiting_config_response_) {
    if (this->available() >= 4) {
      uint8_t response[4];
      for (int i = 0; i < 4; i++) {
        response[i] = this->read();
      }
      
      if (this->validate_config_response_(response, 4)) {
        ESP_LOGI(TAG, "Config command ACK received: 16 01 50 %02X", response[3]);
        this->awaiting_config_response_ = false;
      } else {
        ESP_LOGW(TAG, "Invalid config response: %02X %02X %02X %02X", 
                 response[0], response[1], response[2], response[3]);
        // Clear UART buffer on invalid response
        while (this->available() >= 1) {
          this->read();
        }
      }
    } else if (millis() - this->config_cmd_time_ > 2000) {
      // Timeout waiting for config response
      ESP_LOGW(TAG, "Config response timeout after 2s");
      this->awaiting_config_response_ = false;
    }
    return;  // Don't process sensor data until config is done
  }
  if (millis() - this->last_frame_time_ > this->measurement_period_) {
    if (!this->timeout_active_) {
      ESP_LOGW(TAG, "CM1106SLNS Timeout: no data received for >%ums", this->measurement_period_);
      if (this->error_sensor_ != nullptr)
        this->error_sensor_->publish_state(true);
      this->timeout_active_ = true;
      this->last_frame_time_ = millis();
    }
  } else {
    if (this->timeout_active_) {
      this->timeout_active_ = false;
    }
  }

  while (this->available() >= 8) {
    for (int i = 0; i < 8; i++)
      buffer[i] = this->read();

    if (this->debug_uart_)
      ESP_LOGD(TAG, "UART frame received: %02X %02X %02X %02X %02X %02X %02X %02X",
               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);

    this->last_frame_time_ = millis();
    if (this->timeout_active_) {
      this->timeout_active_ = false;
    }
    if (this->error_sensor_ != nullptr)
      this->error_sensor_->publish_state(false);

    if (!this->validate_checksum_(buffer, 8)) {
      // Log checksum details for debugging
      if (this->debug_uart_) {
        uint8_t sum = 0;
        for (size_t i = 0; i < 7; i++) {
          sum += buffer[i];
        }
        uint8_t expected = (~sum) + 1;
        ESP_LOGW(TAG, "Invalid checksum: expected 0x%02X got 0x%02X (sum 0x%02X)", expected, buffer[7], sum);
        ESP_LOGW(TAG, "Frame bytes: %02X %02X %02X %02X %02X %02X %02X %02X",
                 buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
      }

      this->bad_frames_++;
      if (this->bad_frames_ > 5) {
        this->soft_reset_();
        this->bad_frames_ = 0;
      }
      continue;
    }

    this->bad_frames_ = 0;

    uint16_t co2 = (buffer[3] << 8) | buffer[4];
    uint8_t df3 = buffer[5];
    uint8_t df4 = buffer[6];

    if (this->df3_sensor_ != nullptr)
      this->df3_sensor_->publish_state(df3);
    if (this->df4_sensor_ != nullptr)
      this->df4_sensor_->publish_state(df4);

    auto status = this->interpret_status_(df3, df4);
    // if (this->status_sensor_ != nullptr)
    //   this->status_sensor_->publish_state(status);

    if (this->debug_uart_)
      ESP_LOGD(TAG, "Parsed values: CO2=%u DF3=0x%02X DF4=0x%02X Status=%s", co2, df3, df4,
               status.c_str());

    if (status == "Warming up") {
      if (this->ready_sensor_ != nullptr)
        this->ready_sensor_->publish_state(false);

      if (this->warmup_start_ == 0)
        this->warmup_start_ = millis();

      if (millis() - this->warmup_start_ > this->warmup_timeout_) {
        this->soft_reset_();
        this->warmup_start_ = millis();
      }
      continue;
    }

    this->warmup_start_ = 0;
    if (this->ready_sensor_ != nullptr)
      this->ready_sensor_->publish_state(true);

    // Validate CO2 range
    if (co2 == 0 || co2 < 300 || co2 > 5000)
      continue;

    // Calculate stability counter
    if (std::abs((int)co2 - (int)this->last_valid_co2_) < 20)
      this->stability_counter_ = std::min<uint8_t>(100, this->stability_counter_ + 1);
    else
      this->stability_counter_ = (this->stability_counter_ > 2) ? this->stability_counter_ - 2 : 0;

    if (this->stability_sensor_ != nullptr)
      this->stability_sensor_->publish_state(this->stability_counter_);

    this->last_valid_co2_ = co2;

    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(co2);

    this->publish_iaq_(co2);
  }
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS:");
  LOG_SENSOR(" ", "CO2", this->co2_sensor_);
  LOG_SENSOR(" ", "DF3", this->df3_sensor_);
  LOG_SENSOR(" ", "DF4", this->df4_sensor_);
  //LOG_TEXT_SENSOR(" ", "Status", this->status_sensor_);
  LOG_SENSOR(" ", "Stability", this->stability_sensor_);
  LOG_BINARY_SENSOR(" ", "Ready", this->ready_sensor_);
  LOG_BINARY_SENSOR(" ", "Error", this->error_sensor_);
  LOG_SENSOR(" ", "IAQ Numeric", this->iaq_numeric_);
  //LOG_TEXT_SENSOR(" ", "IAQ Text", this->iaq_text_);
  ESP_LOGCONFIG(TAG, "UART debug: %s", this->debug_uart_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "Measurement period: %ums", this->measurement_period_);
  ESP_LOGCONFIG(TAG, "Warmup timeout: %ums", this->warmup_timeout_);
  ESP_LOGCONFIG(TAG, "Config period: %u seconds", this->config_period_s_);
  ESP_LOGCONFIG(TAG, "Smoothing samples: %u", this->smoothing_samples_);
  ESP_LOGCONFIG(TAG, "Expected UART baud_rate: %u", 9600);
  this->check_uart_settings(9600);
}

void CM1106SLNSComponent::send_config_command_() {
  // Command format: 11 04 50 [DF1] [DF2] [DF3] [CS]
  // DF1 = period_s / 256
  // DF2 = period_s % 256
  // DF3 = smoothing_samples
  
  uint8_t df1 = this->config_period_s_ / 256;
  uint8_t df2 = this->config_period_s_ % 256;
  uint8_t df3 = this->smoothing_samples_;
  
  uint8_t cmd[7] = {0x11, 0x04, 0x50, df1, df2, df3, 0x00};
  cmd[6] = this->calculate_checksum_(cmd, 6);
  
  ESP_LOGI(TAG, "Sending config command: period=%us, smoothing=%u", 
           this->config_period_s_, this->smoothing_samples_);
  ESP_LOGD(TAG, "Command: 11 04 50 %02X %02X %02X %02X", df1, df2, df3, cmd[6]);
  
  this->write_array(cmd, 7);
}

bool CM1106SLNSComponent::validate_config_response_(const uint8_t *buffer, size_t len) {
  // Expected response: 16 01 50 [CS]
  if (len != 4) return false;
  if (buffer[0] != 0x16) return false;
  if (buffer[1] != 0x01) return false;
  if (buffer[2] != 0x50) return false;
  
  return this->validate_checksum_(buffer, len);
}

}  // namespace cm1106sl_ns
}  // namespace esphome
