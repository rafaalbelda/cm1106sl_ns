#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace cm1106sl_ns {

// CM1106SL-NS register protocol from the datasheet.
static const uint8_t CM1106SL_NS_I2C_ADDRESS = 0x34;
static const uint8_t REG_ERROR_STATUS = 0x01;
static const uint8_t REG_CO2_HIGH = 0x06;
static const uint8_t REG_START_SINGLE_MEASUREMENT = 0x93;
static const uint8_t REG_MEASUREMENT_MODE = 0x95;
static const uint8_t REG_MEASUREMENT_PERIOD_HIGH = 0x96;
static const uint32_t SINGLE_MEASUREMENT_DELAY_MS = 3000;

class CM1106SLNSComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_stability_sensor(sensor::Sensor *stability_sensor) { this->stability_sensor_ = stability_sensor; }
  void set_ready_sensor(binary_sensor::BinarySensor *ready_sensor) { this->ready_sensor_ = ready_sensor; }
  void set_error_sensor(binary_sensor::BinarySensor *error_sensor) { this->error_sensor_ = error_sensor; }
  void set_iaq_numeric_sensor(sensor::Sensor *iaq_numeric) { this->iaq_numeric_ = iaq_numeric; }
  void set_status_sensor(sensor::Sensor *status_sensor) { this->status_sensor_ = status_sensor; }
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_measurement_period(uint32_t period) { this->measurement_period_ = period; }
  void set_single_measurement_mode(bool single_mode) { this->single_mode_ = single_mode; }
  void set_internal_measurement_period(uint32_t period_ms);

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *stability_sensor_{nullptr};
  binary_sensor::BinarySensor *ready_sensor_{nullptr};
  binary_sensor::BinarySensor *error_sensor_{nullptr};
  sensor::Sensor *iaq_numeric_{nullptr};
  sensor::Sensor *status_sensor_{nullptr};

  uint16_t last_valid_co2_ = 0;
  uint8_t stability_counter_ = 0;
  uint32_t last_request_time_ = 0;
  uint32_t measurement_start_time_ = 0;
  uint8_t error_count_ = 0;
  uint8_t last_status_ = 0;
  bool debug_ = false;
  bool single_mode_ = true;
  bool timeout_active_ = false;
  bool read_pending_ = false;
  uint32_t measurement_period_ = 60000;  // 60 seconds default
  uint16_t internal_measurement_period_s_ = 120;  // Datasheet default for continuous mode

  void publish_iaq_(uint16_t co2);
  void set_error_(bool error);
  bool configure_measurement_mode_();
  bool write_register_bytes_(uint8_t reg, const uint8_t *data, size_t len);
  bool start_single_measurement_();
  bool read_register_bytes_(uint8_t reg, uint8_t *data, size_t len);
  bool read_measurement_();
  bool publish_measurement_(uint16_t co2, uint8_t status);
};

}  // namespace cm1106sl_ns
}  // namespace esphome

