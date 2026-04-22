#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace cm1106sl_ns {

// CM1106SL-NS extension: adds continuous mode configuration to standard CM1106
// Reference: UART_COMMUNICATION.md - Modo Continuo
class CM1106SLNSComponent : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_config_period(uint16_t period_s) { this->config_period_s_ = period_s; }
  void set_smoothing_samples(uint8_t samples) { this->smoothing_samples_ = samples; }

 protected:
  sensor::Sensor *co2_sensor_{nullptr};
  uint16_t config_period_s_ = 4;         // config period in seconds (1-65535s)
  uint8_t smoothing_samples_ = 1;        // number of smoothed data points

  bool cm1106_write_command_(const uint8_t *command, size_t command_len, uint8_t *response, size_t response_len);
  uint8_t cm1106_checksum_(const uint8_t *response, size_t len);
};

}  // namespace cm1106sl_ns
}  // namespace esphome

