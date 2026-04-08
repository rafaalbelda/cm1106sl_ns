#pragma once

#include "esphome.h"
#include "esphome/components/uart/uart.h"
##include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace cm1106sl_ns {

class CM1106SLNSComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_df3_sensor(sensor::Sensor *df3_sensor) { this->df3_sensor_ = df3_sensor; }
  void set_df4_sensor(sensor::Sensor *df4_sensor) { this->df4_sensor_ = df4_sensor; }
  #void set_status_sensor(text_sensor::TextSensor *status_sensor) { this->status_sensor_ = status_sensor; }
  void set_stability_sensor(sensor::Sensor *stability_sensor) { this->stability_sensor_ = stability_sensor; }
  void set_ready_sensor(binary_sensor::BinarySensor *ready_sensor) { this->ready_sensor_ = ready_sensor; }
  void set_error_sensor(binary_sensor::BinarySensor *error_sensor) { this->error_sensor_ = error_sensor; }
  void set_iaq_numeric_sensor(sensor::Sensor *iaq_numeric) { this->iaq_numeric_ = iaq_numeric; }
  #void set_iaq_text_sensor(text_sensor::TextSensor *iaq_text) { this->iaq_text_ = iaq_text; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *df3_sensor_{nullptr};
  sensor::Sensor *df4_sensor_{nullptr};
  #text_sensor::TextSensor *status_sensor_{nullptr};
  sensor::Sensor *stability_sensor_{nullptr};
  binary_sensor::BinarySensor *ready_sensor_{nullptr};
  binary_sensor::BinarySensor *error_sensor_{nullptr};
  sensor::Sensor *iaq_numeric_{nullptr};
  #text_sensor::TextSensor *iaq_text_{nullptr};

  uint16_t last_valid_co2_ = 0;
  uint8_t stability_counter_ = 0;
  uint32_t last_frame_time_ = 0;
  uint32_t warmup_start_ = 0;
  uint8_t bad_frames_ = 0;

  std::string interpret_status_(uint8_t df3, uint8_t df4);
  bool validate_checksum_(const uint8_t *buffer, size_t len);
  void publish_iaq_(uint16_t co2);
  void soft_reset_();
};

}  // namespace cm1106sl_ns
}  // namespace esphome

