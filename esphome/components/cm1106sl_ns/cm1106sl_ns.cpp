#include "cm1106sl_ns.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

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
  // In continuous mode, sensor automatically sends 8-byte data frames
  // Frame format: [0x16][CMD][0x50][CO2H][CO2L][DF3][DF4][CS]
  // We read continuous frames from the UART buffer (no explicit read command needed)
  
  // Look for complete frame in buffer (8 bytes starting with 0x16)
  while (this->available() >= 8) {
    uint8_t frame[8] = {0};
    
    // Find start byte (0x16)
    uint8_t start_byte = this->read();
    if (start_byte != 0x16) {
      ESP_LOGD(TAG, "Skipping byte 0x%02X, waiting for frame start 0x16", start_byte);
      continue;  // Keep looking for frame start
    }
    
    // Read remaining 7 bytes
    frame[0] = start_byte;
    if (!this->read_array(&frame[1], 7)) {
      ESP_LOGW(TAG, "Failed to read complete frame from buffer");
      return;
    }
    
    // Validate checksum
    uint8_t expected_checksum = cm1106_checksum_(frame, sizeof(frame));
    if (frame[7] != expected_checksum) {
      ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X, got 0x%02X, frame: [0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X]",
               expected_checksum, frame[7], frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6], frame[7]);
      continue;  // Try next frame
    }
    
    this->status_clear_warning();
    
    // Extract CO2 value - sensor sends continuous frame with CO2 at bytes 3-4
    // Continuous frame format: [0x16][CMD_ECHO][0x50][CO2H][CO2L][DF3][DF4][CS]
    uint16_t co2 = (frame[3] << 8) | frame[4];
    uint8_t status = frame[5];
    uint8_t info = frame[6];
    
    ESP_LOGD(TAG, "CO2=%uppm, Status=0x%02X, Info=0x%02X", co2, status, info);
    
    if (this->co2_sensor_ != nullptr) {
      this->co2_sensor_->publish_state(co2);
    }
    
    return;  // Frame processed successfully
  }
  
  // No complete frame available yet
  if (!this->available()) {
    ESP_LOGD(TAG, "No data available in buffer");
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
