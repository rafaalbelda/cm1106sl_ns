#include "cm1106sl_ns.h"
#include "esphome/core/log.h"
#include <cstring>
#include <cmath>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

void CM1106SLNSComponent::setup() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS sensor setup");
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
  if (this->iaq_text_ != nullptr)
    this->iaq_text_->publish_state(label);
}

void CM1106SLNSComponent::soft_reset_() {
  ESP_LOGW(TAG, "Soft reset del sensor");
  const uint8_t reset_cmd[5] = {0x11, 0x03, 0x02, 0x00, 0xED};
  this->write_array(reset_cmd, 5);
}

void CM1106SLNSComponent::update() {
  uint8_t buffer[8];

  // Check for timeout
  if (millis() - this->last_frame_time_ > 15000) {
    if (this->error_sensor_ != nullptr)
      this->error_sensor_->publish_state(true);
  }

  while (this->available() >= 8) {
    for (int i = 0; i < 8; i++)
      buffer[i] = this->read();

    this->last_frame_time_ = millis();
    if (this->error_sensor_ != nullptr)
      this->error_sensor_->publish_state(false);

    if (!this->validate_checksum_(buffer, 8)) {
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
    if (this->status_sensor_ != nullptr)
      this->status_sensor_->publish_state(status);

    if (status == "Warming up") {
      if (this->ready_sensor_ != nullptr)
        this->ready_sensor_->publish_state(false);

      if (this->warmup_start_ == 0)
        this->warmup_start_ = millis();

      if (millis() - this->warmup_start_ > 60000) {
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
  #LOG_TEXT_SENSOR(" ", "Status", this->status_sensor_);
  LOG_SENSOR(" ", "Stability", this->stability_sensor_);
  LOG_BINARY_SENSOR(" ", "Ready", this->ready_sensor_);
  LOG_BINARY_SENSOR(" ", "Error", this->error_sensor_);
  LOG_SENSOR(" ", "IAQ Numeric", this->iaq_numeric_);
  #LOG_TEXT_SENSOR(" ", "IAQ Text", this->iaq_text_);
  this->check_uart_settings(9600);
}

}  // namespace cm1106sl_ns
}  // namespace esphome
