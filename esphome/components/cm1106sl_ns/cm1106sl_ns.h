#pragma once

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
//#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace cm1106sl_ns {

// UART Protocol: CM1106SL-NS CO2 Sensor
// See UART_COMMUNICATION.md for protocol details
//
// Data Frame Format (8 bytes):
//   [0x16][0x05][0x50][CO2H][CO2L][DF3][DF4][CS]
//   - 0x16 0x05: Data frame header
//   - 0x50: Command byte (read data)
//   - CO2H/CO2L: CO2 concentration in ppm (16-bit, big-endian)
//   - DF3: Sensor status (0x00=Normal, 0x08=Warming up, 0x01=Error, 0x02=Calibration needed)
//   - DF4: Additional information byte
//   - CS: Two's complement checksum

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

  // Protocol constants from UART_COMMUNICATION.md
  static constexpr uint8_t FRAME_HEADER_1 = 0x16;      // First byte of data frame
  static constexpr uint8_t FRAME_HEADER_2 = 0x05;      // Second byte of data frame
  static constexpr uint8_t FRAME_COMMAND = 0x50;       // Command byte for data frame
  static constexpr uint8_t CONFIG_RESPONSE_BYTE_1 = 0x16;
  static constexpr uint8_t CONFIG_RESPONSE_BYTE_2 = 0x01;
  static constexpr uint8_t CONFIG_RESPONSE_CMD = 0x50;
  static constexpr uint8_t FRAME_LENGTH = 8;           // Data frame is 8 bytes
  static constexpr uint8_t CONFIG_RESPONSE_LENGTH = 4; // Config response is 4 bytes
  static constexpr uint8_t RESET_CMD_LENGTH = 5;       // Reset command is 5 bytes
  static constexpr uint16_t CO2_MIN_VALID = 300;       // Minimum valid CO2 ppm
  static constexpr uint16_t CO2_MAX_VALID = 5000;      // Maximum valid CO2 ppm
  static constexpr uint32_t CONFIG_RESPONSE_TIMEOUT = 2000;  // 2 seconds
  static constexpr uint32_t DATA_TIMEOUT_MARGIN = 500;  // ms margin over expected period
  static constexpr uint8_t MAX_BAD_FRAMES = 5;         // Reset after 5 bad frames
  static constexpr uint8_t STABILITY_THRESHOLD = 20;   // ppm difference for stability
  static constexpr uint32_t WARMUP_STATUS_VALUE = 0x08; // DF3 value for warming up

  uint16_t last_valid_co2_ = 0;
  uint8_t stability_counter_ = 0;
  uint32_t last_frame_time_ = 0;
  uint32_t warmup_start_ = 0;
  uint8_t bad_frames_ = 0;
  bool debug_uart_ = false;
  bool timeout_active_ = false;
  uint32_t measurement_period_ = 15000;  // 15 seconds
  uint32_t warmup_timeout_ = 60000;      // 60 seconds
  uint16_t config_period_s_ = 4;         // config period in seconds (1-65535s)
  uint8_t smoothing_samples_ = 1;        // number of smoothed data points
  bool awaiting_config_response_ = true;
  uint32_t config_cmd_time_ = 0;
  bool config_command_sent_ = false;     // tracks delayed config send after soft reset

  // Continuous Mode Configuration
  // Similar to Arduino: setupCM1106() detects current mode and configures
  // References:
  //   - Arduino: my_cm1106.ino setupCM1106() / get_working_status() / set_working_status()
  //   - UART: UART_COMMUNICATION.md - Modo Continuo
  static constexpr uint32_t CONFIG_RETRY_DELAY = 1000;    // Retry config after 1 second
  static constexpr uint8_t MAX_CONFIG_RETRIES = 5;        // Maximum retry attempts
  
  uint8_t config_retry_count_ = 0;
  uint32_t config_retry_time_ = 0;
  bool continuous_mode_confirmed_ = false;

  std::string interpret_status_(uint8_t df3, uint8_t df4);
  void send_config_command_();
  void check_config_retry_();
  bool validate_config_response_(const uint8_t *buffer, size_t len);
  bool validate_frame_header_(const uint8_t *buffer, size_t len);
  bool validate_checksum_(const uint8_t *buffer, size_t len);
  uint8_t calculate_checksum_(const uint8_t *buffer, size_t len);
  void publish_iaq_(uint16_t co2);
  void soft_reset_();
};

}  // namespace cm1106sl_ns
}  // namespace esphome

