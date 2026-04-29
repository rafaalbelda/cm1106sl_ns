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
static const uint8_t REG_CO2_HIGH = 0x06;
static const uint8_t REG_START_SINGLE_MEASUREMENT = 0x93;
static const uint8_t REG_MEASUREMENT_MODE = 0x95;
static const uint32_t SINGLE_MEASUREMENT_DELAY_MS = 3000;

// CM1106 command-oriented I2C protocol used by the cm1106_i2s/cm1106_i2c library.
static const uint8_t CM1106_COMMAND_I2C_ADDRESS = 0x31;
static const uint8_t CM1106_CMD_MEASURE_RESULT = 0x01;
static const uint32_t CM1106_DELAY_FOR_ACK_MS = 500;

// Status byte values returned by the library protocol.
static const uint8_t CM1106_STATUS_PREHEATING = 0x00;
static const uint8_t CM1106_STATUS_NORMAL_OPERATION = 0x01;
static const uint8_t CM1106_STATUS_OPERATING_TROUBLE = 0x02;
static const uint8_t CM1106_STATUS_OUT_OF_FS = 0x03;
static const uint8_t CM1106_STATUS_NON_CALIBRATED = 0x05;

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
  void set_command_protocol(bool command_protocol) { this->command_protocol_ = command_protocol; }

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
  uint32_t command_sent_time_ = 0;
  uint8_t error_count_ = 0;
  uint8_t last_status_ = 0;
  bool debug_ = false;
  bool command_protocol_ = false;
  bool timeout_active_ = false;
  bool read_pending_ = false;
  uint32_t measurement_period_ = 60000;  // 60 seconds default

  void publish_iaq_(uint16_t co2);
  void set_error_(bool error);
  static uint8_t checksum_(const uint8_t *data, size_t len);
  bool request_measurement_();
  bool start_single_measurement_();
  bool read_register_bytes_(uint8_t reg, uint8_t *data, size_t len);
  bool read_measurement_();
  bool read_register_measurement_();
  bool publish_measurement_(uint16_t co2, uint8_t status);
};

}  // namespace cm1106sl_ns
}  // namespace esphome

