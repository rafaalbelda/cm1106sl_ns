#include "cm1106sl_ns.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

uint8_t CM1106SLNSComponent::cm1106_checksum_(const uint8_t *response, size_t len) {
  // Two's complement checksum: sum all bytes except checksum, then (~sum) + 1
  uint8_t sum = 0;
  for (size_t i = 0; i < len - 1; i++) {
    sum += response[i];
  }
  return (~sum) + 1;
}

void CM1106SLNSComponent::setup() {
  // Initialization: First detect and set working mode to continuous, then configure period/smoothing
  // Reference: Arduino my_cm1106.ino setupCM1106() / UART_COMMUNICATION.md
  
  ESP_LOGCONFIG(TAG, "=== CM1106SL-NS Setup ===");
  
  // Step 1: Detect current working mode
  ESP_LOGCONFIG(TAG, "Step 1: Detecting sensor working mode...");
  uint8_t current_mode = 0xFF;
  if (!this->cm1106_get_working_status_(&current_mode)) {
    ESP_LOGE(TAG, "Failed to detect sensor working mode");
    this->mark_failed();
    return;
  }
  
  // Display detected mode
  const char *mode_str = (current_mode == 0x00) ? "Single Measurement" : 
                         (current_mode == 0x01) ? "Continuous Measurement" : 
                         "Unknown/Invalid";
  ESP_LOGCONFIG(TAG, "Current mode: %s (0x%02X)", mode_str, current_mode);
  
  // Step 2: Switch to continuous mode if not already in it
  if (current_mode != 0x01) {  // 0x01 = Continuous Measurement
    ESP_LOGCONFIG(TAG, "Step 2: Switching to continuous mode...");
    if (!this->cm1106_set_working_status_(0x01)) {
      ESP_LOGE(TAG, "Failed to set continuous mode");
      this->mark_failed();
      return;
    }
    ESP_LOGCONFIG(TAG, "Successfully changed to continuous mode");
  } else {
    ESP_LOGCONFIG(TAG, "Step 2: Already in continuous mode - skipping mode change");
  }
  
  // Step 3: Configure period and smoothing (only if in continuous mode)
  ESP_LOGCONFIG(TAG, "Step 3: Configuring continuous mode: period=%us, smoothing=%u samples", 
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
    ESP_LOGE(TAG, "Failed to send configuration command");
    this->mark_failed();
    return;
  }
  
  // Check response: should be [0x16][0x01][0x50][CS]
  if (response[0] == 0x16 && response[1] == 0x01 && response[2] == 0x50) {
    uint8_t expected_cs = this->cm1106_checksum_(response, 4);
    if (response[3] == expected_cs) {
      ESP_LOGCONFIG(TAG, "Configuration successful - sensor ready for continuous data streaming");
    } else {
      ESP_LOGW(TAG, "Configuration response checksum mismatch: expected 0x%02X, got 0x%02X", 
               expected_cs, response[3]);
    }
  } else {
    ESP_LOGW(TAG, "Unexpected configuration response: 0x%02X 0x%02X 0x%02X 0x%02X", 
             response[0], response[1], response[2], response[3]);
  }
  
  ESP_LOGCONFIG(TAG, "Setup complete - waiting for continuous data frames");
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
    // Only log every 30 seconds to avoid spam
    static uint32_t last_log_time = 0;
    uint32_t now = millis();
    if (now - last_log_time > 30000) {
      ESP_LOGD(TAG, "No data available in buffer (waiting for continuous frames)");
      last_log_time = now;
    }
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

bool CM1106SLNSComponent::cm1106_get_working_status_(uint8_t *mode) {
  // Get current working mode from sensor
  // Reference: UART_COMMUNICATION.md - Comando GET working status (0x51)
  // Command: [0x11][0x01][0x51][CS] (4 bytes)
  // Response: [0x16][0x02][0x51][MODE][CS] (5 bytes)
  // MODE: 0x00 = Single Measurement, 0x01 = Continuous Measurement
  
  if (mode == nullptr) {
    return false;
  }
  
  uint8_t cmd[4] = {0x11, 0x01, 0x51, 0x00};  // Comando correcto: 0x51
  cmd[3] = this->cm1106_checksum_(cmd, 4);
  
  ESP_LOGD(TAG, "Sending GET mode command: 0x11 0x01 0x51 0x%02X", cmd[3]);
  
  uint8_t response[5] = {0};
  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGE(TAG, "Failed to read sensor working status");
    return false;
  }
  
  // Validate response: [0x16][0x02][0x51][MODE][CS]
  if (response[0] != 0x16 || response[1] != 0x02 || response[2] != 0x51) {  // Echo correcto: 0x51
    ESP_LOGW(TAG, "Invalid GET mode response: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2], response[3], response[4]);
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = this->cm1106_checksum_(response, 5);
  if (response[4] != expected_cs) {
    ESP_LOGW(TAG, "GET mode response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[4]);
    return false;
  }
  
  *mode = response[3];
  ESP_LOGD(TAG, "GET mode response: mode=0x%02X", *mode);
  return true;
}

bool CM1106SLNSComponent::cm1106_set_working_status_(uint8_t mode) {
  // Set working mode on sensor
  // Reference: UART_COMMUNICATION.md - Comando SET working status (0x51)
  // Command: [0x11][0x02][0x51][MODE][CS] (5 bytes)  ← Longitud=0x02, Comando=0x51
  // Response: [0x16][0x01][0x51][CS] (4 bytes)        ← Echo=0x51
  // MODE: 0x00 = Single Measurement, 0x01 = Continuous Measurement
  
  if (mode != 0x00 && mode != 0x01) {
    ESP_LOGE(TAG, "Invalid mode value: 0x%02X (must be 0x00 or 0x01)", mode);
    return false;
  }
  
  uint8_t cmd[5] = {0x11, 0x02, 0x51, mode, 0x00};  // Longitud=0x02, Comando=0x51
  cmd[4] = this->cm1106_checksum_(cmd, 5);
  
  const char *mode_str = (mode == 0x00) ? "Single Measurement" : "Continuous Measurement";
  ESP_LOGD(TAG, "Sending SET mode command to %s: 0x11 0x02 0x51 0x%02X 0x%02X", 
           mode_str, mode, cmd[4]);
  
  uint8_t response[4] = {0};
  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGE(TAG, "Failed to set sensor working status");
    return false;
  }
  
  // Validate response: [0x16][0x01][0x51][CS]
  if (response[0] != 0x16 || response[1] != 0x01 || response[2] != 0x51) {  // Echo correcto: 0x51
    ESP_LOGW(TAG, "Invalid SET mode response: 0x%02X 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2], response[3]);
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = this->cm1106_checksum_(response, 4);
  if (response[3] != expected_cs) {
    ESP_LOGW(TAG, "SET mode response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[3]);
    return false;
  }
  
  ESP_LOGD(TAG, "SET mode response: mode changed to %s", mode_str);
  return true;
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

  ESP_LOGE(TAG, "Setup:");
  this->setup();


}

}  // namespace cm1106sl_ns
}  // namespace esphome
