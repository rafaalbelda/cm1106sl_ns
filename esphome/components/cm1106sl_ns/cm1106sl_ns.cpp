#include "cm1106sl_ns.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

// Data read command: [0x11][0x01][0x01][CS]
static const uint8_t CMD_GET_CO2[4] = {0x11, 0x01, 0x01, 0xED};

uint8_t CM1106SLNSComponent::cm1106_checksum_(const uint8_t *response, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len - 1; i++) {
    crc -= response[i];
  }
  return crc;
}

void CM1106SLNSComponent::setup() {
  // Initialization: Send command to set continuous mode with configured period and smoothing
  // Reference: Arduino my_cm1106.ino setupCM1106() / UART_COMMUNICATION.md
  
  ESP_LOGCONFIG(TAG, "=== CM1106SL-NS Setup ===");
  ESP_LOGCONFIG(TAG, "Configuring continuous mode: period=%us, smoothing=%u samples", 
                this->config_period_s_, this->smoothing_samples_);
  
  // Build configuration command: [0x11][0x04][0x50][DF1][DF2][DF3][CS]
  // DF1 = Period_s / 256 (MSB)
  // DF2 = Period_s % 256 (LSB)
  // DF3 = Smoothing samples
  uint8_t df1 = this->config_period_s_ / 256;
  uint8_t df2 = this->config_period_s_ % 256;
  uint8_t df3 = this->smoothing_samples_;
  
  uint8_t cmd[7] = {0x11, 0x04, 0x50, df1, df2, df3, 0x00};
  cmd[6] = this->cm1106_checksum_(cmd, 7);
  
  ESP_LOGCONFIG(TAG, "Sending config command: 0x11 0x04 0x50 0x%02X 0x%02X 0x%02X 0x%02X", 
                df1, df2, df3, cmd[6]);
  
  uint8_t response[4] = {0};
  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGE(TAG, "Failed to configure continuous mode");
    this->mark_failed();
    return;
  }
  
  ESP_LOGCONFIG(TAG, "Configuration sent successfully");
}

void CM1106SLNSComponent::update() {
  // Read CO2 data from sensor
  // Frame format: [0x16][0x05][0x01][CO2H][CO2L][DF3][DF4][CS]
  // Reference: Arduino my_cm1106.ino readCM1106()
  
  uint8_t response[8] = {0};
  if (!this->cm1106_write_command_(CMD_GET_CO2, sizeof(CMD_GET_CO2), 
                                    response, sizeof(response))) {
    ESP_LOGW(TAG, "Failed to read CO2 data from sensor");
    this->status_set_warning();
    return;
  }

  // Validate response header
  if (response[0] != 0x16 || response[1] != 0x05 || response[2] != 0x01) {
    ESP_LOGW(TAG, "Invalid response header: 0x%02X 0x%02X 0x%02X", 
             response[0], response[1], response[2]);
    this->status_set_warning();
    return;
  }

  // Validate checksum
  uint8_t checksum = cm1106_checksum_(response, sizeof(response));
  if (response[7] != checksum) {
    ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X", 
             checksum, response[7]);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  // Extract CO2 value (16-bit, big-endian)
  uint16_t co2 = (response[3] << 8) | response[4];
  
  ESP_LOGD(TAG, "CO2=%uppm, DF3=0x%02X, DF4=0x%02X", co2, response[5], response[6]);
  
  if (this->co2_sensor_ != nullptr) {
    this->co2_sensor_->publish_state(co2);
  }
}

bool CM1106SLNSComponent::cm1106_write_command_(const uint8_t *command, size_t command_len, 
                                                uint8_t *response, size_t response_len) {
  // Clear RX buffer
  while (this->available()) {
    this->read();
  }
  
  // Send command
  this->write_array(command, command_len - 1);
  this->write_byte(cm1106_checksum_(command, command_len));
  this->flush();

  if (response == nullptr)
    return true;

  // Read response
  return this->read_array(response, response_len);
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS (Continuous Mode):");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  ESP_LOGCONFIG(TAG, "  Config Period: %u seconds", this->config_period_s_);
  ESP_LOGCONFIG(TAG, "  Smoothing Samples: %u", this->smoothing_samples_);
  this->check_uart_settings(9600);
    if (this->is_failed()) {
    ESP_LOGE(TAG, "Failed connecting to sensor");
  }
}

}  // namespace cm1106sl_ns
}  // namespace esphome
