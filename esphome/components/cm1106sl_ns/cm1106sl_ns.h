#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace cm1106sl_ns {

// CM1106SL-NS I2C Configuration
static const uint8_t CM1106_I2C_ADDRESS = 0x34;  // 7-bit address, default from datasheet

// I2C Registers
static const uint8_t REG_CO2_HIGH = 0x06;                  // CO2 concentration high byte
static const uint8_t REG_CO2_LOW = 0x07;                   // CO2 concentration low byte
static const uint8_t REG_START_SINGLE_MEASUREMENT = 0x93;  // Write 0x01 to start one single measurement
static const uint8_t REG_MEASUREMENT_MODE = 0x95;          // 0x00=continuous, 0x01=single
static const uint32_t SINGLE_MEASUREMENT_DELAY_MS = 3000;  // Conservative delay when RDY is not connected
static const uint32_t INITIAL_CONFIGURE_DELAY_MS = 10000;
static const uint32_t CONFIGURE_RETRY_INTERVAL_MS = 120000;

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
  void set_debug(bool debug) { this->debug_ = debug; }
  void set_measurement_period(uint32_t period) { this->measurement_period_ = period; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *stability_sensor_{nullptr};
  binary_sensor::BinarySensor *ready_sensor_{nullptr};
  binary_sensor::BinarySensor *error_sensor_{nullptr};
  sensor::Sensor *iaq_numeric_{nullptr};

  uint16_t last_valid_co2_ = 0;
  uint8_t stability_counter_ = 0;
  uint32_t last_frame_time_ = 0;
  uint32_t measurement_start_time_ = 0;
  uint32_t next_configure_time_ = 0;
  uint8_t error_count_ = 0;
  bool debug_ = false;
  bool timeout_active_ = false;
  bool measurement_pending_ = false;
  bool configured_ = false;
  uint32_t measurement_period_ = 60000;  // 60 seconds default

  void publish_iaq_(uint16_t co2);
  void set_error_(bool error);
  bool configure_sensor_();
  bool start_single_measurement_();
  bool read_register_bytes_(uint8_t reg, uint8_t *data, size_t len);
  bool read_measurement_();
};

}  // namespace cm1106sl_ns
}  // namespace esphome

