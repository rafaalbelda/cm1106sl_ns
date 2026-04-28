#ifndef CM1106SL_NS
#define CM1106SL_NS

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

  void calibrate_zero(uint16_t ppm);

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_config_period(uint16_t period_s) { this->config_period_s_ = period_s; }
  void set_response_timeout(uint32_t response_timeout_ms) { this->response_timeout_ms_ = response_timeout_ms; }
  void set_smoothing_samples(uint8_t samples) { this->smoothing_samples_ = samples; }

 protected:
  enum class InitState : uint8_t {
    READ_SOFTWARE_VERSION,
    READ_SERIAL_NUMBER,
    GET_WORKING_STATUS,
    SET_WORKING_STATUS,
    GET_MEASUREMENT_PERIOD,
    SET_MEASUREMENT_PERIOD,
    DONE,
    FAILED,
  };

  enum class TransactionOperation : uint8_t {
    NONE,
    READ_CO2,
    CALIBRATE_ZERO,
    INIT_READ_SOFTWARE_VERSION,
    INIT_READ_SERIAL_NUMBER,
    INIT_GET_WORKING_STATUS,
    INIT_SET_WORKING_STATUS,
    INIT_GET_MEASUREMENT_PERIOD,
    INIT_SET_MEASUREMENT_PERIOD,
  };

  sensor::Sensor *co2_sensor_{nullptr};
  uint16_t config_period_s_ = 4;         // config period in seconds (1-65535s)
  uint32_t response_timeout_ms_{4000};
  uint8_t smoothing_samples_ = 1;        // number of smoothed data points
  bool initialized_ = false;
  bool init_logged_ = false;
  InitState init_state_{InitState::READ_SOFTWARE_VERSION};
  uint8_t current_mode_{0xFF};
  uint16_t current_period_{0};
  uint8_t current_smoothing_{0};
  uint16_t calibration_ppm_{400};

  TransactionOperation pending_operation_{TransactionOperation::NONE};
  uint8_t response_buffer_[15]{};
  size_t response_len_{0};
  uint32_t response_deadline_{0};

  void setupCM1106_();
  bool start_transaction_(TransactionOperation operation, const uint8_t *command, size_t command_len,
                          size_t response_len);
  bool transaction_pending_() const { return this->pending_operation_ != TransactionOperation::NONE; }
  void poll_response_();
  void finish_transaction_(bool success);
  void handle_transaction_timeout_(TransactionOperation operation);
  void process_response_(TransactionOperation operation);
  void fail_initialization_();
  void request_co2_();
  void request_calibration_(uint16_t ppm);
  void request_software_version_();
  void request_serial_number_();
  void request_working_status_();
  void request_set_working_status_(uint8_t mode);
  void request_measurement_period_();
  void request_set_measurement_period_(uint16_t period, uint8_t smoothing);
  bool validate_response_header_(uint8_t length, uint8_t command, const char *label);
  bool validate_response_checksum_(const char *label, size_t response_len);
  bool process_co2_response_();
  bool process_calibration_response_();
  bool process_software_version_response_();
  bool process_serial_number_response_();
  bool process_working_status_response_();
  bool process_set_working_status_response_();
  bool process_measurement_period_response_();
  bool process_set_measurement_period_response_();
};

template<typename... Ts> class CM1106CalibrateZeroAction : public Action<Ts...> {
 public:
  CM1106CalibrateZeroAction(CM1106SLNSComponent *cm1106) : cm1106_(cm1106) {}

  void play(const Ts &...x) override { this->cm1106_->calibrate_zero(400); }

 protected:
  CM1106SLNSComponent *cm1106_;
};

}  // namespace cm1106sl_ns
}  // namespace esphome


#endif /* CM1106SL_NS */
