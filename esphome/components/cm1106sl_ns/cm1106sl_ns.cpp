#include "cm1106sl_ns.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";
//static const uint32_t CM1106_SERIAL_READ_TIMEOUT_MS = 4000;
static const uint8_t C_M1106_CMD_GET_CO2[4] = {0x11, 0x01, 0x01, 0xED};
static const uint8_t C_M1106_CMD_SET_CO2_CALIB[6] = {0x11, 0x03, 0x03, 0x00, 0x00, 0x00};
static const uint8_t C_M1106_CMD_SET_CO2_CALIB_RESPONSE[4] = {0x16, 0x01, 0x03, 0xE6};

uint8_t cm1106_checksum(const uint8_t *response, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len - 1; i++) {
    crc -= response[i];
  }
  return crc;
}

void CM1106SLNSComponent::setup() {
//  this->setupCM1106_();
}


void CM1106SLNSComponent::setupCM1106_() {
  if (this->initialized_ || this->transaction_pending_() || this->init_state_ == InitState::FAILED) {
    return;
  }

  if (!this->init_logged_) {
    ESP_LOGCONFIG(TAG, "-== CM1106SL-NS Initialization ==-");
    this->init_logged_ = true;
  }

  switch (this->init_state_) {
    case InitState::READ_SOFTWARE_VERSION:
      ESP_LOGCONFIG(TAG, "Step 0: Reading sensor information...");
      this->request_software_version_();
      break;
    case InitState::READ_SERIAL_NUMBER:
      this->request_serial_number_();
      break;
    case InitState::GET_WORKING_STATUS:
      ESP_LOGCONFIG(TAG, "Step 1: Detecting sensor working mode...");
      this->request_working_status_();
      break;
    case InitState::SET_WORKING_STATUS:
      this->request_set_working_status_(0x01);
      break;
    case InitState::GET_MEASUREMENT_PERIOD:
      ESP_LOGCONFIG(TAG, "Step 3: Reading current measurement configuration...");
      this->request_measurement_period_();
      break;
    case InitState::SET_MEASUREMENT_PERIOD:
      this->request_set_measurement_period_(this->config_period_s_, this->smoothing_samples_);
      break;
    case InitState::DONE:
      this->initialized_ = true;
      ESP_LOGCONFIG(TAG, "Initialization complete - sensor ready for continuous data streaming");
      break;
    case InitState::FAILED:
      break;
  }
}

void CM1106SLNSComponent::update() {

  if (!(this->initialized_ || this->transaction_pending_() || this->init_state_ == InitState::FAILED)) {
    this->setupCM1106_();
  }

  if (this->transaction_pending_()) {
    return;
  }

  if (!this->initialized_) {
    this->setupCM1106_();
    return;
  }

  this->request_co2_();
}

bool CM1106SLNSComponent::start_transaction_(TransactionOperation operation, const uint8_t *command, size_t command_len,
                                             size_t response_len) {
  if (this->transaction_pending_()) {
    ESP_LOGD(TAG, "Skipping command while UART transaction is pending");
    return false;
  }
  if (command == nullptr || command_len == 0 || response_len > sizeof(this->response_buffer_)) {
    return false;
  }

  while (this->available())
    this->read();

  memset(this->response_buffer_, 0, sizeof(this->response_buffer_));
  this->pending_operation_ = operation;
  this->response_len_ = response_len;
  this->response_deadline_ = millis() + this->response_timeout_ms_;

  this->write_array(command, command_len - 1);
  this->write_byte(cm1106_checksum(command, command_len));
  this->flush();

  if (response_len == 0) {
    this->finish_transaction_(true);
    return true;
  }

  this->set_interval("cm1106_read_response", 20, [this]() { this->poll_response_(); });
  return true;
}

void CM1106SLNSComponent::poll_response_() {
  if (!this->transaction_pending_()) {
    this->cancel_interval("cm1106_read_response");
    return;
  }

  TransactionOperation operation = this->pending_operation_;
  if (this->available() >= static_cast<int>(this->response_len_)) {
    this->read_array(this->response_buffer_, this->response_len_);
    this->finish_transaction_(true);
    this->process_response_(operation);
    return;
  }

  if (static_cast<int32_t>(millis() - this->response_deadline_) >= 0) {
    this->finish_transaction_(false);
    this->handle_transaction_timeout_(operation);
  }
}

void CM1106SLNSComponent::finish_transaction_(bool success) {
  this->cancel_interval("cm1106_read_response");
  this->pending_operation_ = TransactionOperation::NONE;
  this->response_len_ = 0;
  if (!success) {
    memset(this->response_buffer_, 0, sizeof(this->response_buffer_));
  }
}

void CM1106SLNSComponent::handle_transaction_timeout_(TransactionOperation operation) {
  ESP_LOGW(TAG, "Timeout waiting for CM1106 response after %u ms", static_cast<unsigned>(this->response_timeout_ms_));
  this->status_set_warning();

  switch (operation) {
    case TransactionOperation::INIT_READ_SOFTWARE_VERSION:
      ESP_LOGW(TAG, "  Failed to read software version");
      this->init_state_ = InitState::READ_SERIAL_NUMBER;
      this->setupCM1106_();
      break;
    case TransactionOperation::INIT_READ_SERIAL_NUMBER:
      ESP_LOGW(TAG, "  Failed to read serial number");
      this->init_state_ = InitState::GET_WORKING_STATUS;
      this->setupCM1106_();
      break;
    case TransactionOperation::INIT_GET_MEASUREMENT_PERIOD:
      ESP_LOGW(TAG, "  Failed to read current measurement period");
      break;
    case TransactionOperation::INIT_GET_WORKING_STATUS:
    case TransactionOperation::INIT_SET_WORKING_STATUS:
    case TransactionOperation::INIT_SET_MEASUREMENT_PERIOD:
      this->fail_initialization_();
      break;
    case TransactionOperation::READ_CO2:
      ESP_LOGW(TAG, "Reading data from CM1106 failed!");
      break;
    case TransactionOperation::CALIBRATE_ZERO:
      ESP_LOGW(TAG, "CM1106 calibration timed out");
      break;
    case TransactionOperation::NONE:
      break;
  }
}

void CM1106SLNSComponent::process_response_(TransactionOperation operation) {
  bool success = false;
  switch (operation) {
    case TransactionOperation::READ_CO2:
      success = this->process_co2_response_();
      break;
    case TransactionOperation::CALIBRATE_ZERO:
      success = this->process_calibration_response_();
      break;
    case TransactionOperation::INIT_READ_SOFTWARE_VERSION:
      success = this->process_software_version_response_();
      this->init_state_ = InitState::READ_SERIAL_NUMBER;
      this->setupCM1106_();
      break;
    case TransactionOperation::INIT_READ_SERIAL_NUMBER:
      success = this->process_serial_number_response_();
      this->init_state_ = InitState::GET_WORKING_STATUS;
      this->setupCM1106_();
      break;
    case TransactionOperation::INIT_GET_WORKING_STATUS:
      success = this->process_working_status_response_();
      if (success) {
        const char *mode_str = (this->current_mode_ == 0x00) ? "Single Measurement" :
                               (this->current_mode_ == 0x01) ? "Continuous Measurement" :
                               "Unknown/Invalid";
        ESP_LOGCONFIG(TAG, "  Current mode: %s (0x%02X)", mode_str, this->current_mode_);
        ESP_LOGCONFIG(TAG, "Step 2: Switching to continuous mode...");
        if (this->current_mode_ != 0x01) {
          this->init_state_ = InitState::SET_WORKING_STATUS;
        } else {
          ESP_LOGCONFIG(TAG, "  Already in continuous mode - skipping mode change");
          this->init_state_ = InitState::GET_MEASUREMENT_PERIOD;
        }
        this->setupCM1106_();
      } else {
        ESP_LOGE(TAG, "  Failed to detect sensor working mode");
        this->fail_initialization_();
      }
      break;
    case TransactionOperation::INIT_SET_WORKING_STATUS:
      success = this->process_set_working_status_response_();
      if (success) {
        ESP_LOGCONFIG(TAG, "  Successfully changed to continuous mode");
        this->init_state_ = InitState::GET_MEASUREMENT_PERIOD;
        this->setupCM1106_();
      } else {
        ESP_LOGE(TAG, "  Failed to set continuous mode");
        this->fail_initialization_();
      }
      break;
    case TransactionOperation::INIT_GET_MEASUREMENT_PERIOD:
      success = this->process_measurement_period_response_();
      if (success) {
        ESP_LOGCONFIG(TAG, "Step 4: Checking if configuration update is needed...");
        ESP_LOGCONFIG(TAG, "  Current settings: period=%u seconds, smoothing=%u samples", this->current_period_,
                      this->current_smoothing_);
        if (this->current_period_ != this->config_period_s_ || this->current_smoothing_ != this->smoothing_samples_) {
          ESP_LOGCONFIG(TAG, "  Configuration differs - updating sensor settings...");
          ESP_LOGCONFIG(TAG, "  Target: period=%u seconds, smoothing=%u samples", this->config_period_s_,
                        this->smoothing_samples_);
          this->init_state_ = InitState::SET_MEASUREMENT_PERIOD;
        } else {
          ESP_LOGCONFIG(TAG, "  Measurement Period matches - no changes needed");
          this->init_state_ = InitState::DONE;
        }
        this->setupCM1106_();
      } else {
        ESP_LOGW(TAG, "  Failed to read current measurement period");
        this->status_set_warning();
      }
      break;
    case TransactionOperation::INIT_SET_MEASUREMENT_PERIOD:
      success = this->process_set_measurement_period_response_();
      if (success) {
        ESP_LOGCONFIG(TAG, "  Measurement Period updated successfully");
        this->init_state_ = InitState::DONE;
        this->setupCM1106_();
      } else {
        ESP_LOGE(TAG, "  Failed to update measurement period");
        this->fail_initialization_();
      }
      break;
    case TransactionOperation::NONE:
      break;
  }

  if (!success) {
    this->status_set_warning();
  }
}

void CM1106SLNSComponent::fail_initialization_() {
  this->init_state_ = InitState::FAILED;
  this->mark_failed();
}

void CM1106SLNSComponent::request_co2_() {
  this->start_transaction_(TransactionOperation::READ_CO2, C_M1106_CMD_GET_CO2, sizeof(C_M1106_CMD_GET_CO2), 8);
}

void CM1106SLNSComponent::request_calibration_(uint16_t ppm) {
  uint8_t cmd[6];
  memcpy(cmd, C_M1106_CMD_SET_CO2_CALIB, sizeof(cmd));
  cmd[3] = ppm >> 8;
  cmd[4] = ppm & 0xFF;
  this->calibration_ppm_ = ppm;
  this->start_transaction_(TransactionOperation::CALIBRATE_ZERO, cmd, sizeof(cmd), 4);
}

void CM1106SLNSComponent::request_software_version_() {
  uint8_t cmd[4] = {0x11, 0x01, 0x1E, 0x00};
  ESP_LOGD(TAG, "  Sending GET software version command: 0x11 0x01 0x1E 0x%02X", cm1106_checksum(cmd, sizeof(cmd)));
  this->start_transaction_(TransactionOperation::INIT_READ_SOFTWARE_VERSION, cmd, sizeof(cmd), 15);
}

void CM1106SLNSComponent::request_serial_number_() {
  uint8_t cmd[4] = {0x11, 0x01, 0x1F, 0x00};
  ESP_LOGD(TAG, "  Sending GET serial number command: 0x11 0x01 0x1F 0x%02X", cm1106_checksum(cmd, sizeof(cmd)));
  this->start_transaction_(TransactionOperation::INIT_READ_SERIAL_NUMBER, cmd, sizeof(cmd), 14);
}

void CM1106SLNSComponent::request_working_status_() {
  uint8_t cmd[4] = {0x11, 0x01, 0x51, 0x00};
  ESP_LOGD(TAG, "  Sending GET mode command: 0x11 0x01 0x51 0x%02X", cm1106_checksum(cmd, sizeof(cmd)));
  this->start_transaction_(TransactionOperation::INIT_GET_WORKING_STATUS, cmd, sizeof(cmd), 5);
}

void CM1106SLNSComponent::request_set_working_status_(uint8_t mode) {
  uint8_t cmd[5] = {0x11, 0x02, 0x51, mode, 0x00};
  const char *mode_str = (mode == 0x00) ? "Single Measurement" : "Continuous Measurement";
  ESP_LOGD(TAG, "  Sending SET mode command to %s: 0x11 0x02 0x51 0x%02X 0x%02X", mode_str, mode,
           cm1106_checksum(cmd, sizeof(cmd)));
  this->start_transaction_(TransactionOperation::INIT_SET_WORKING_STATUS, cmd, sizeof(cmd), 4);
}

void CM1106SLNSComponent::request_measurement_period_() {
  uint8_t cmd[4] = {0x11, 0x01, 0x50, 0x00};
  ESP_LOGD(TAG, "  Sending GET measurement period command: 0x11 0x01 0x50 0x%02X",
           cm1106_checksum(cmd, sizeof(cmd)));
  this->start_transaction_(TransactionOperation::INIT_GET_MEASUREMENT_PERIOD, cmd, sizeof(cmd), 8);
}

void CM1106SLNSComponent::request_set_measurement_period_(uint16_t period, uint8_t smoothing) {
  uint8_t period_h = (period >> 8) & 0xFF;
  uint8_t period_l = period & 0xFF;
  uint8_t cmd[7] = {0x11, 0x04, 0x50, period_h, period_l, smoothing, 0x00};
  ESP_LOGD(TAG, "  Sending SET measurement period command: 0x11 0x04 0x50 0x%02X 0x%02X 0x%02X 0x%02X", period_h,
           period_l, smoothing, cm1106_checksum(cmd, sizeof(cmd)));
  this->start_transaction_(TransactionOperation::INIT_SET_MEASUREMENT_PERIOD, cmd, sizeof(cmd), 4);
}

bool CM1106SLNSComponent::validate_response_header_(uint8_t length, uint8_t command, const char *label) {
  if (this->response_buffer_[0] == 0x16 && this->response_buffer_[1] == length &&
      this->response_buffer_[2] == command) {
    return true;
  }

  ESP_LOGW(TAG, "  Invalid %s response: 0x%02X 0x%02X 0x%02X", label, this->response_buffer_[0],
           this->response_buffer_[1], this->response_buffer_[2]);
  return false;
}

bool CM1106SLNSComponent::validate_response_checksum_(const char *label, size_t response_len) {
  uint8_t expected_cs = cm1106_checksum(this->response_buffer_, response_len);
  if (this->response_buffer_[response_len - 1] == expected_cs) {
    return true;
  }

  ESP_LOGW(TAG, "  %s response checksum mismatch: expected 0x%02X, got 0x%02X", label, expected_cs,
           this->response_buffer_[response_len - 1]);
  return false;
}

bool CM1106SLNSComponent::process_co2_response_() {
  if (!this->validate_response_header_(0x05, 0x01, "CO2") || !this->validate_response_checksum_("CO2", 8)) {
    ESP_LOGW(TAG, "Reading data from CM1106 failed!");
    return false;
  }

  this->status_clear_warning();

  uint16_t ppm = response_buffer_[3] << 8 | response_buffer_[4];
  ESP_LOGD(TAG, "CM1106 Received CO₂=%uppm DF3=%02X DF4=%02X", ppm, response_buffer_[5], response_buffer_[6]);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(ppm);
  return true;
}

bool CM1106SLNSComponent::process_calibration_response_() {
  if (memcmp(this->response_buffer_, C_M1106_CMD_SET_CO2_CALIB_RESPONSE, 4) != 0) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X", response_buffer_[0],
             response_buffer_[1], response_buffer_[2], response_buffer_[3]);
    return false;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "CM1106 Successfully calibrated sensor to %uppm", this->calibration_ppm_);
  return true;
}

bool CM1106SLNSComponent::process_software_version_response_() {
  if (!this->validate_response_header_(0x0C, 0x1E, "software version") ||
      !this->validate_response_checksum_("Software version", 15)) {
    ESP_LOGW(TAG, "  Failed to read software version");
    return false;
  }

  char version[11] = {0};
  strncpy(version, (const char *) &this->response_buffer_[3], sizeof(version) - 1);
  ESP_LOGCONFIG(TAG, "  Software Version: %s", version);
  return true;
}

bool CM1106SLNSComponent::process_serial_number_response_() {
  if (!this->validate_response_header_(0x0B, 0x1F, "serial number") ||
      !this->validate_response_checksum_("Serial number", 14)) {
    ESP_LOGW(TAG, "  Failed to read serial number");
    return false;
  }

  char serial[21] = {0};
  size_t pos = 0;
  for (int i = 0; i < 5 && pos + 4 < sizeof(serial); i++) {
    uint16_t sn_part = ((this->response_buffer_[3 + 2 * i] & 0xFF) << 8) | (this->response_buffer_[4 + 2 * i] & 0xFF);
    pos += snprintf(&serial[pos], sizeof(serial) - pos, "%04u", sn_part);
  }

  ESP_LOGCONFIG(TAG, "  Serial Number: %s", serial);
  this->status_clear_warning();
  return true;
}

bool CM1106SLNSComponent::process_working_status_response_() {
  if (!this->validate_response_header_(0x02, 0x51, "GET mode") ||
      !this->validate_response_checksum_("GET mode", 5)) {
    return false;
  }

  this->current_mode_ = this->response_buffer_[3];
  this->status_clear_warning();
  ESP_LOGD(TAG, "  GET mode response: mode=0x%02X", this->current_mode_);
  return true;
}

bool CM1106SLNSComponent::process_set_working_status_response_() {
  if (!this->validate_response_header_(0x01, 0x51, "SET working mode") ||
      !this->validate_response_checksum_("SET working mode", 4)) {
    return false;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "  SET working mode response: mode changed to Continuous Measurement");
  return true;
}

bool CM1106SLNSComponent::process_measurement_period_response_() {
  if (!this->validate_response_header_(0x04, 0x50, "measurement period") ||
      !this->validate_response_checksum_("Measurement period", 8)) {
    return false;
  }

  this->current_period_ = (this->response_buffer_[3] << 8) | this->response_buffer_[4];
  this->current_smoothing_ = this->response_buffer_[5];
  this->status_clear_warning();
  ESP_LOGD(TAG, "  GET measurement period: period=%u seconds, smoothing=%u samples", this->current_period_,
           this->current_smoothing_);
  return true;
}

bool CM1106SLNSComponent::process_set_measurement_period_response_() {
  if (!this->validate_response_header_(0x01, 0x50, "SET measurement period") ||
      !this->validate_response_checksum_("SET measurement period", 4)) {
    return false;
  }

  ESP_LOGD(TAG, "  SET measurement period: period=%u seconds, smoothing=%u samples", this->config_period_s_,
           this->smoothing_samples_);
  this->status_clear_warning();
  return true;
}

void CM1106SLNSComponent::calibrate_zero(uint16_t ppm) {
  if (this->transaction_pending_()) {
    ESP_LOGW(TAG, "Cannot calibrate CM1106 while another UART transaction is pending");
    return;
  }
  this->request_calibration_(ppm);
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  ESP_LOGCONFIG(TAG, "  Config Period: %u seconds", this->config_period_s_);
  ESP_LOGCONFIG(TAG, "  Smoothing Samples: %u", this->smoothing_samples_);
  ESP_LOGCONFIG(TAG, "  Response Timeout: %u ms", static_cast<unsigned>(this->response_timeout_ms_));
  this->check_uart_settings(9600);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  } else {
    ESP_LOGCONFIG(TAG, "Sensor connection successful");

    // setup is not executed so we include the call here to ensure the sensor is properly initialized and configured before use
    //this->setup();
  }
}

}  // namespace cm1106sl_ns
}  // namespace esphome
