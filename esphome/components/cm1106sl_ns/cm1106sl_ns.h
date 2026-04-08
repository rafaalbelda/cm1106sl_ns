#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
//#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace cm1106sl_ns {

class CM1106SLNSComponent : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_df3_sensor(sensor::Sensor *df3_sensor) { this->df3_sensor_ = df3_sensor; }
  void set_df4_sensor(sensor::Sensor *df4_sensor) { this->df4_sensor_ = df4_sensor; }
  //void set_status_sensor(text_sensor::TextSensor *status_sensor) { this->status_sensor_ = status_sensor; }
  void set_stability_sensor(sensor::Sensor *stability_sensor) { this->stability_sensor_ = stability_sensor; }
  void set_ready_sensor(binary_sensor::BinarySensor *ready_sensor) { this->ready_sensor_ = ready_sensor; }
  void set_error_sensor(binary_sensor::BinarySensor *error_sensor) { this->error_sensor_ = error_sensor; }
  void set_iaq_numeric_sensor(sensor::Sensor *iaq_numeric) { this->iaq_numeric_ = iaq_numeric; }
  //void set_iaq_text_sensor(text_sensor::TextSensor *iaq_text) { this->iaq_text_ = iaq_text; }
  void set_debug_uart(bool debug) { this->debug_uart_ = debug; }
  void set_measurement_period(uint32_t period) { this->measurement_period_ = period; }
  void set_warmup_timeout(uint32_t timeout) { this->warmup_timeout_ = timeout; }
  void set_config_period(uint16_t period_s) { this->config_period_s_ = period_s; }
  void set_smoothing_samples(uint8_t samples) { this->smoothing_samples_ = samples; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *df3_sensor_{nullptr};
  sensor::Sensor *df4_sensor_{nullptr};
  //text_sensor::TextSensor *status_sensor_{nullptr};
  sensor::Sensor *stability_sensor_{nullptr};
  binary_sensor::BinarySensor *ready_sensor_{nullptr};
  binary_sensor::BinarySensor *error_sensor_{nullptr};
  sensor::Sensor *iaq_numeric_{nullptr};
  //text_sensor::TextSensor *iaq_text_{nullptr};

  uint16_t last_valid_co2_ = 0;
  uint8_t stability_counter_ = 0;
  uint32_t last_frame_time_ = 0;
  uint32_t warmup_start_ = 0;
  uint8_t bad_frames_ = 0;
  bool debug_uart_ = false;
  bool timeout_active_ = false;
  uint32_t measurement_period_ = 15000;  // 15 seconds
  uint32_t warmup_timeout_ = 60000;      // 60 seconds
  uint16_t config_period_s_ = 4;         // config period in seconds (4-600s)
  uint8_t smoothing_samples_ = 1;        // number of smoothed data points
  bool awaiting_config_response_ = true;
  uint32_t config_cmd_time_ = 0;
  bool config_command_sent_ = false;

  std::string interpret_status_(uint8_t df3, uint8_t df4);
  void send_config_command_();
  bool validate_config_response_(const uint8_t *buffer, size_t len);
  bool validate_checksum_(const uint8_t *buffer, size_t len);
  uint8_t calculate_checksum_(const uint8_t *buffer, size_t len);
  void publish_iaq_(uint16_t co2);
  void soft_reset_();
};

}  // namespace cm1106sl_ns
}  // namespace esphome

