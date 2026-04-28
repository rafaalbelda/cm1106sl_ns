#include "cm1106sl_ns.h"
#include "esphome/core/log.h"

#include <cinttypes>

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
  // Initialization moved to update() on first call
  uint8_t response[8] = {0};
  if (!this->cm1106_write_command_(C_M1106_CMD_GET_CO2, sizeof(C_M1106_CMD_GET_CO2), response, sizeof(response))) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
    this->mark_failed();
    return;
  }
}


void CM1106SLNSComponent::setupCM1106_() {
  if (!this->initialized_) {
    // Initialization: First detect and set working mode to continuous, then configure period/smoothing
    // Reference: Arduino my_cm1106.ino setupCM1106() / UART_COMMUNICATION.md
    
    ESP_LOGCONFIG(TAG, "-== CM1106SL-NS Initialization ==-");

    if (!this->initialized0_) {
      // Step 0: Read sensor identification (version and serial number)
      ESP_LOGCONFIG(TAG, "Step 0: Reading sensor information...");
      
      char version_buf[16] = {0};
      if (this->cm1106_get_software_version_(version_buf, sizeof(version_buf))) {
        ESP_LOGCONFIG(TAG, "  Software Version: %s", version_buf);
      } else {
        ESP_LOGW(TAG, "  Failed to read software version");
      }
      
      char serial_buf[32] = {0};
      if (this->cm1106_get_serial_number_(serial_buf, sizeof(serial_buf))) {
        ESP_LOGCONFIG(TAG, "  Serial Number: %s", serial_buf);
      } else {
        ESP_LOGW(TAG, "  Failed to read serial number");
        this->status_set_warning();
      }

      // Step 1: Detect current working mode and switch to continuous if needed
      ESP_LOGCONFIG(TAG, "Step 1: Detecting sensor working mode...");
      uint8_t current_mode = 0xFF;
      if (!this->cm1106_get_working_status_(&current_mode)) {
        ESP_LOGE(TAG, "  Failed to detect sensor working mode");
        this->mark_failed();
        this->initialized_ = true;
        return;
      }
      
      // Display detected mode
      const char *mode_str = (current_mode == 0x00) ? "Single Measurement" : 
                             (current_mode == 0x01) ? "Continuous Measurement" : 
                             "Unknown/Invalid";
      ESP_LOGCONFIG(TAG, "  Current mode: %s (0x%02X)", mode_str, current_mode);

      // Step 2: Switch to continuous mode if not already in it
      ESP_LOGCONFIG(TAG, "Step 2: Switching to continuous mode...");
      if (current_mode != 0x01) {  // 0x01 = Continuous Measurement
        if (!this->cm1106_set_working_status_(0x01)) {
          ESP_LOGE(TAG, "Failed to set continuous mode");
          this->mark_failed();
          this->initialized_ = true;
          return;
        }
        ESP_LOGCONFIG(TAG, "  Successfully changed to continuous mode");
      } else {
        ESP_LOGCONFIG(TAG, "  Already in continuous mode - skipping mode change");
      }
      this->initialized0_ = true;
    }
    
    if (this->initialized1_ > 0) {
      // Step 3: Read current measurement period and smoothing settings
      ESP_LOGCONFIG(TAG, "Step 3: Reading current measurement configuration...");
      uint16_t current_period = 0;
      uint8_t current_smoothing = 0;
      
      if (!this->cm1106_get_measurement_period_(&current_period, &current_smoothing)) {
        ESP_LOGW(TAG, "  Failed to read current measurement period");
        this->initialized1_--;
        this->status_set_warning();
        return;
      } else {
        ESP_LOGCONFIG(TAG, "  Current settings: period=%u seconds, smoothing=%u samples", 
                      current_period, current_smoothing);
      }
    
      // Step 4: Update measurement period and smoothing if different from desired configuration
      ESP_LOGCONFIG(TAG, "Step 4: Checking if configuration update is needed...");
      // Step 4: Update period and smoothing only if different from current values
      if (current_period != this->config_period_s_ || current_smoothing != this->smoothing_samples_) {
        ESP_LOGCONFIG(TAG, "  Configuration differs - updating sensor settings...");
        ESP_LOGCONFIG(TAG, "  Target: period=%u seconds, smoothing=%u samples", 
                      this->config_period_s_, this->smoothing_samples_);
        
        if (!this->cm1106_set_measurement_period_(this->config_period_s_, this->smoothing_samples_)) {
          ESP_LOGE(TAG, "Failed to update measurement period");
          this->mark_failed();
          this->initialized_ = true;
          return;
        }
        ESP_LOGCONFIG(TAG, "Configuration updated successfully");
      } else {
        ESP_LOGCONFIG(TAG, "Step 4: Configuration matches - no changes needed");
      }
      this->initialized1_ = 0;
    }
    
    ESP_LOGCONFIG(TAG, "Initialization complete - sensor ready for continuous data streaming");
    this->initialized_ = this->initialized0_ && (this->initialized1_ == 0);
  }

}

void CM1106SLNSComponent::update() {

  this->setupCM1106_();

  uint8_t response[8] = {0};
  if (!this->cm1106_write_command_(C_M1106_CMD_GET_CO2, sizeof(C_M1106_CMD_GET_CO2), response, sizeof(response))) {
    ESP_LOGW(TAG, "Reading data from CM1106 failed!");
    this->status_set_warning();
    return;
  }

  if (response[0] != 0x16 || response[1] != 0x05 || response[2] != 0x01) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X", response[0], response[1], response[2],
             response[3]);
    this->status_set_warning();
    return;
  }

  uint8_t checksum = cm1106_checksum(response, sizeof(response));
  if (response[7] != checksum) {
    ESP_LOGW(TAG, "CM1106 Checksum doesn't match: 0x%02X!=0x%02X", response[7], checksum);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  uint16_t ppm = response[3] << 8 | response[4];
  ESP_LOGD(TAG, "CM1106 Received CO₂=%uppm DF3=%02X DF4=%02X", ppm, response[5], response[6]);
  if (this->co2_sensor_ != nullptr)
    this->co2_sensor_->publish_state(ppm);
}

// bool CM1106SLNSComponent::cm1106_serial_read_bytes(uint8_t *buffer, size_t len, uint32_t timeout_ms) {
//   if (buffer == nullptr || len == 0) {
//     return false;
//   }

//   uint32_t deadline = millis() + timeout_ms;

//   // Synchronize: discard any junk bytes until we find 0x16 (response start marker)
//   while (millis() < deadline) {
//     if (!this->available()) {
//       delay(100);
//       continue;
//     }

//     uint8_t byte = this->read();
//     if (byte != 0x16) {
//       continue;
//     }

//     buffer[0] = byte;
//     size_t idx = 1;
//     while (idx < len && millis() < deadline) {
//       if (!this->available()) {
//         delay(100);
//         continue;
//       }
//       buffer[idx++] = this->read();
//     }

//     return idx == len;
//   }

//   ESP_LOGW(TAG, "Timeout waiting for response marker (0x16)");
//   return false;
//}

// bool CM1106SLNSComponent::cm1106_write_command_(const uint8_t *command, size_t command_len) {
//   if (command == nullptr || command_len == 0) {
//     return false;
//   }

//   // Clear RX buffer completely
//   while (this->available()) {
//     this->read();
//   }

//   // Send command bytes excluding checksum placeholder
//   this->write_array(command, command_len - 1);
//   // Send checksum
//   this->write_byte(cm1106_checksum_(command, command_len));
//   // Ensure all bytes are sent
//   this->flush();

//   return true;
// }

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
  cmd[3] = cm1106_checksum(cmd, 4);
  
  ESP_LOGD(TAG, "  Sending GET mode command: 0x11 0x01 0x51 0x%02X", cmd[3]);
  
  uint8_t response[5] = {0};
  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "  Failed to read sensor working status!");
    this->status_set_warning();
    return false;
  }
  // if (!this->cm1106_write_command_(cmd, sizeof(cmd))) {
  //   ESP_LOGE(TAG, "Failed to send GET mode command");
  //   return false;
  // }
  // if (!this->cm1106_serial_read_bytes(response, sizeof(response), CM1106_SERIAL_READ_TIMEOUT_MS)) {
  //   ESP_LOGE(TAG, "Failed to read sensor working status");
  //   return false;
  // }
  
  // Validate response: [0x16][0x02][0x51][MODE][CS]
  if (response[0] != 0x16 || response[1] != 0x02 || response[2] != 0x51) {  // Echo correcto: 0x51
    ESP_LOGW(TAG, "  Invalid GET mode response: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2], response[3], response[4]);
    this->status_set_warning();
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = cm1106_checksum(response, 5);
  if (response[4] != expected_cs) {
    ESP_LOGW(TAG, "  GET mode response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[4]);
    this->status_set_warning();
    return false;
  }
  
  *mode = response[3];
  this->status_clear_warning();
  ESP_LOGD(TAG, "  GET mode response: mode=0x%02X", *mode);
  return true;
}

bool CM1106SLNSComponent::cm1106_set_working_status_(uint8_t mode) {
  // Set working mode on sensor
  // Reference: UART_COMMUNICATION.md - Comando SET working status (0x51)
  // Command: [0x11][0x02][0x51][MODE][CS] (5 bytes)  ← Longitud=0x02, Comando=0x51
  // Response: [0x16][0x01][0x51][CS] (4 bytes)        ← Echo=0x51
  // MODE: 0x00 = Single Measurement, 0x01 = Continuous Measurement
  
  if (mode != 0x00 && mode != 0x01) {
    ESP_LOGE(TAG, "  Invalid mode value: 0x%02X (must be 0x00 or 0x01)", mode);
    return false;
  }
  
  uint8_t cmd[5] = {0x11, 0x02, 0x51, mode, 0x00};  // Longitud=0x02, Comando=0x51
  cmd[4] = cm1106_checksum(cmd, 5);
  
  const char *mode_str = (mode == 0x00) ? "Single Measurement" : "Continuous Measurement";
  ESP_LOGD(TAG, "  Sending SET mode command to %s: 0x11 0x02 0x51 0x%02X 0x%02X", 
           mode_str, mode, cmd[4]);
  
  uint8_t response[4] = {0};
  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "  Failed to send SET working mode!");
    this->status_set_warning();
    return false;
  }
  // if (!this->cm1106_write_command_(cmd, sizeof(cmd))) {
  //   ESP_LOGE(TAG, "Failed to send SET mode command");
  //   return false;
  // }
  // if (!this->cm1106_serial_read_bytes(response, sizeof(response), CM1106_SERIAL_READ_TIMEOUT_MS)) {
  //   ESP_LOGE(TAG, "Failed to read SET mode response");
  //   return false;
  // }
  
  // Validate response: [0x16][0x01][0x51][CS]
  if (response[0] != 0x16 || response[1] != 0x01 || response[2] != 0x51) {  // Echo correcto: 0x51
    ESP_LOGW(TAG, "  Invalid SET working mode response: 0x%02X 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2], response[3]);
    this->status_set_warning();
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = cm1106_checksum(response, 4);
  if (response[3] != expected_cs) {
    ESP_LOGW(TAG, "  SET working mode response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[3]);
    this->status_set_warning();
    return false;
  }
  
  this->status_clear_warning();
  ESP_LOGD(TAG, "  SET working mode response: mode changed to %s", mode_str);
  return true;
}

bool CM1106SLNSComponent::cm1106_get_software_version_(char *version, size_t len) {
  // Get software version from sensor
  // Command: [0x11][0x01][0x1E][CS] (4 bytes)
  // Response: [0x16][0x0C][0x1E][VERSION_DATA...][CS] (15 bytes total)
  
  if (version == nullptr || len == 0) {
    return false;
  }
  
  memset(version, 0, len);
  
  uint8_t cmd[4] = {0x11, 0x01, 0x1E, 0x00};  // Comando: 0x1E (GET_SOFTWARE_VERSION)
  cmd[3] = cm1106_checksum(cmd, 4);
  
  ESP_LOGD(TAG, "  Sending GET software version command: 0x11 0x01 0x1E 0x%02X", cmd[3]);
  
  uint8_t response[15] = {0};
  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "  Failed to read software version!");
    this->status_set_warning();
    return false ;
  }
  // if (!this->cm1106_write_command_(cmd, sizeof(cmd))) {
  //   ESP_LOGE(TAG, "Failed to send GET software version command");
  //   return false;
  // }

  // if (!this->cm1106_serial_read_bytes(response, sizeof(response), CM1106_SERIAL_READ_TIMEOUT_MS)) {
  //   ESP_LOGE(TAG, "Failed to read software version");
  //   return false;
  // }
  
  // Validate response: [0x16][0x0C][0x1E][VERSION...][CS]
  if (response[0] != 0x16 || response[1] != 0x0C || response[2] != 0x1E) {
    ESP_LOGW(TAG, "  Invalid software version response: 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2]);
    this->status_set_warning();
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = cm1106_checksum(response, sizeof(response));
  if (response[14] != expected_cs) {
    ESP_LOGW(TAG, "  Software version response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[14]);
    this->status_set_warning();
    return false;
  }
  
  // Extract version string from bytes 3-12 (10 bytes max)
  size_t copy_len = (len - 1 < 10) ? len - 1 : 10;
  strncpy(version, (const char *)&response[3], copy_len);
  version[copy_len] = '\0';
  
  this->status_clear_warning();
  ESP_LOGD(TAG, "  GET software version: %s", version);
  return true;
}

bool CM1106SLNSComponent::cm1106_get_serial_number_(char *serial, size_t len) {
  // Get serial number from sensor
  // Command: [0x11][0x01][0x1F][CS] (4 bytes)
  // Response: [0x16][0x0B][0x1F][SN_DATA...][CS] (14 bytes total)
  // Serial number is formatted as 5 groups of 4 digits
  
  if (serial == nullptr || len == 0) {
    return false;
  }
  
  memset(serial, 0, len);
  
  uint8_t cmd[4] = {0x11, 0x01, 0x1F, 0x00};  // Comando: 0x1F (GET_SERIAL_NUMBER)
  cmd[3] = cm1106_checksum(cmd, 4);
  
  ESP_LOGD(TAG, "  Sending GET serial number command: 0x11 0x01 0x1F 0x%02X", cmd[3]);
  
  uint8_t response[14] = {0};
    if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "  Failed to read serial number!");
    this->status_set_warning();
    return false;
  }
  // if (!this->cm1106_write_command_(cmd, sizeof(cmd))) {
  //   ESP_LOGE(TAG, "Failed to send GET serial number command");
  //   return false;
  // }
  // if (!this->cm1106_serial_read_bytes(response, sizeof(response), CM1106_SERIAL_READ_TIMEOUT_MS)) {
  //   ESP_LOGE(TAG, "Failed to read serial number");
  //   return false;
  // }
  
  // Validate response: [0x16][0x0B][0x1F][SN_DATA...][CS]
  if (response[0] != 0x16 || response[1] != 0x0B || response[2] != 0x1F) {
    ESP_LOGW(TAG, "  Invalid serial number response: 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2]);
    this->status_set_warning();
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = cm1106_checksum(response, sizeof(response));
  if (response[13] != expected_cs) {
    ESP_LOGW(TAG, "  Serial number response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[13]);
    this->status_set_warning();
    return false;
  }
  
  // Extract serial number: 5 groups of 4 digits from 10 bytes (2 bytes per group)
  size_t pos = 0;
  for (int i = 0; i < 5 && pos + 4 < len; i++) {
    uint16_t sn_part = ((response[3 + 2*i] & 0xFF) << 8) | (response[4 + 2*i] & 0xFF);
    pos += snprintf(&serial[pos], len - pos, "%04u", sn_part);
  }
  
  this->status_clear_warning();
  ESP_LOGD(TAG, "  GET serial number: %s", serial);
  return true;
}

bool CM1106SLNSComponent::cm1106_get_measurement_period_(uint16_t *period, uint8_t *smoothing) {
  // Get measurement period and smoothing samples from sensor
  // Command: [0x11][0x01][0x50][CS] (4 bytes)
  // Response: [0x16][0x04][0x50][PERIOD_H][PERIOD_L][SMOOTHING][CS] (8 bytes)
  
  if (period == nullptr || smoothing == nullptr) {
    return false;
  }
  
  uint8_t cmd[4] = {0x11, 0x01, 0x50, 0x00};  // Comando: 0x50 (MEASUREMENT_PERIOD GET)
  cmd[3] = cm1106_checksum(cmd, 4);
  
  ESP_LOGD(TAG, "  Sending GET measurement period command: 0x11 0x01 0x50 0x%02X", cmd[3]);
  
  uint8_t response[8] = {0};
    if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "  Failed to read measurement period!");
    this->status_set_warning();
    return false;
  }
  // if (!this->cm1106_write_command_(cmd, sizeof(cmd))) {
  //   ESP_LOGE(TAG, "Failed to send GET measurement period command");
  //   return false;
  // }
  // if (!this->cm1106_serial_read_bytes(response, sizeof(response), CM1106_SERIAL_READ_TIMEOUT_MS)) {
  //   ESP_LOGE(TAG, "Failed to read measurement period");
  //   return false;
  // }
  
  // Validate response: [0x16][0x04][0x50][PERIOD_H][PERIOD_L][SMOOTHING][CS]
  if (response[0] != 0x16 || response[1] != 0x04 || response[2] != 0x50) {
    ESP_LOGW(TAG, "  Invalid measurement period response: 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2]);
    this->status_set_warning();
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = cm1106_checksum(response, sizeof(response));
  if (response[7] != expected_cs) {
    ESP_LOGW(TAG, "  Measurement period response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[7]);
    this->status_set_warning();
    return false;
  }
  
  // Extract period (16-bit value, MSB first) and smoothing
  *period = (response[3] << 8) | response[4];
  *smoothing = response[5];
  
  this->status_clear_warning();
  ESP_LOGD(TAG, "  GET measurement period: period=%u seconds, smoothing=%u samples", *period, *smoothing);
  return true;
}

bool CM1106SLNSComponent::cm1106_set_measurement_period_(uint16_t period, uint8_t smoothing) {
  // Set measurement period and smoothing samples
  // Command: [0x11][0x04][0x50][PERIOD_H][PERIOD_L][SMOOTHING][CS] (7 bytes)
  // Response: [0x16][0x01][0x50][CS] (4 bytes)
  
  uint8_t period_h = (period >> 8) & 0xFF;
  uint8_t period_l = period & 0xFF;
  
  uint8_t cmd[7] = {0x11, 0x04, 0x50, period_h, period_l, smoothing, 0x00};
  cmd[6] = cm1106_checksum(cmd, 7);
  
  ESP_LOGD(TAG, "  Sending SET measurement period command: 0x11 0x04 0x50 0x%02X 0x%02X 0x%02X 0x%02X", 
           period_h, period_l, smoothing, cmd[6]);
  
  uint8_t response[4] = {0};
  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "  Failed to SET measurement period!");
    this->status_set_warning();
    return false;
  }
  // if (!this->cm1106_write_command_(cmd, sizeof(cmd))) {
  //   ESP_LOGE(TAG, "Failed to send SET measurement period command");
  //   return false;
  // }
  // if (!this->cm1106_serial_read_bytes(response, sizeof(response), CM1106_SERIAL_READ_TIMEOUT_MS)) {
  //   ESP_LOGE(TAG, "Failed to read SET measurement period response");
  //   return false;
  // }
  
  // Validate response: [0x16][0x01][0x50][CS]
  if (response[0] != 0x16 || response[1] != 0x01 || response[2] != 0x50) {
    ESP_LOGW(TAG, "  Invalid SET measurement period response: 0x%02X 0x%02X 0x%02X 0x%02X",
             response[0], response[1], response[2], response[3]);
    this->status_set_warning();
    return false;
  }
  
  // Validate checksum
  uint8_t expected_cs = cm1106_checksum(response, sizeof(response));
  if (response[3] != expected_cs) {
    ESP_LOGW(TAG, "  SET measurement period response checksum mismatch: expected 0x%02X, got 0x%02X",
             expected_cs, response[3]);
    this->status_set_warning();
    return false;
  }
  
  ESP_LOGD(TAG, "  SET measurement period: period=%u seconds, smoothing=%u samples", period, smoothing);
  this->status_clear_warning();
  return true;
}

void CM1106SLNSComponent::calibrate_zero(uint16_t ppm) {
  uint8_t cmd[6];
  memcpy(cmd, C_M1106_CMD_SET_CO2_CALIB, sizeof(cmd));
  cmd[3] = ppm >> 8;
  cmd[4] = ppm & 0xFF;
  uint8_t response[4] = {0};

  if (!this->cm1106_write_command_(cmd, sizeof(cmd), response, sizeof(response))) {
    ESP_LOGW(TAG, "Reading data from CM1106 failed!");
    this->status_set_warning();
    return;
  }

  // check if correct response received
  if (memcmp(response, C_M1106_CMD_SET_CO2_CALIB_RESPONSE, sizeof(response)) != 0) {
    ESP_LOGW(TAG, "Got wrong UART response from CM1106: %02X %02X %02X %02X", response[0], response[1], response[2],
             response[3]);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();
  ESP_LOGD(TAG, "CM1106 Successfully calibrated sensor to %uppm", ppm);
}

bool CM1106SLNSComponent::cm1106_write_command_(const uint8_t *command, size_t command_len, uint8_t *response,
                                            size_t response_len) {
  // Empty RX Buffer
  while (this->available())
    this->read();
  this->write_array(command, command_len - 1);
  this->write_byte(cm1106_checksum(command, command_len));
  this->flush();

  if (response == nullptr)
    return true;

  return this->read_array(response, response_len);
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS:");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  ESP_LOGCONFIG(TAG, "  Config Period: %u seconds", this->config_period_s_);
  ESP_LOGCONFIG(TAG, "  Smoothing Samples: %u", this->smoothing_samples_);
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
