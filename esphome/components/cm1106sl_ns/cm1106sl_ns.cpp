#include "cm1106sl_ns.h"
#include "esphome/core/log.h"
#include <cstring>
#include <cmath>

namespace esphome {
namespace cm1106sl_ns {

static const char *const TAG = "cm1106sl_ns";

// Protocol reference: UART_COMMUNICATION.md
// Data frame: [0x16][0x05][0x50][CO2H][CO2L][DF3][DF4][CS]
// Config response: [0x16][0x01][0x50][CS]
// Reset command: [0x11][0x03][0x02][0x00][0xED]

void CM1106SLNSComponent::setup() {
  this->last_frame_time_ = millis();
  this->config_cmd_time_ = millis();
  this->config_retry_time_ = millis();
  
  // Initialization sequence (similar to Arduino setupCM1106)
  // Phase 1: Send initial config command to set continuous mode
  // Reference: Arduino my_cm1106.ino - get_working_status() / set_working_status()
  
  this->awaiting_config_response_ = true;
  this->config_retry_count_ = 0;
  this->continuous_mode_confirmed_ = false;
  
  ESP_LOGCONFIG(TAG, "=== CM1106SL-NS Initialization ===");
  ESP_LOGCONFIG(TAG, "Step 1: Sending initial config command for continuous mode");
  ESP_LOGCONFIG(TAG, "Config: period=%us, smoothing=%u", 
                this->config_period_s_, this->smoothing_samples_);
  
  this->send_config_command_();
  this->config_command_sent_ = true;
}

std::string CM1106SLNSComponent::interpret_status_(uint8_t df3, uint8_t df4) {
  // Reference: UART_COMMUNICATION.md - Estados del Sensor (DF3)
  // DF3 byte values:
  //   0x08 = Warming up
  //   0x00 = Normal
  //   0x01 = Sensor error
  //   0x02 = Calibration required
  
  if (df3 == 0x08)
    return "Warming up";
  if (df3 == 0x00 && df4 == 0x00)
    return "Normal";
  if (df3 == 0x01)
    return "Sensor error";
  if (df3 == 0x02)
    return "Calibration required";
  return "Unknown";
}

bool CM1106SLNSComponent::validate_checksum_(const uint8_t *buffer, size_t len) {
  // Reference: UART_COMMUNICATION.md - Cálculo de Checksum
  // Checksum = Two's complement of sum of all bytes except checksum
  // Formula: CS = (~sum) + 1
  // Validation: If CS is correct, sum of all bytes should equal 0
  
  uint8_t sum = 0;
  for (size_t i = 0; i < len - 1; i++) {
    sum += buffer[i];
  }
  uint8_t expected = (~sum) + 1;
  
  if (expected != buffer[len - 1] && this->debug_uart_) {
    ESP_LOGD(TAG, "Checksum validation: expected 0x%02X got 0x%02X (sum 0x%02X)", 
             expected, buffer[len - 1], sum);
  }
  
  return expected == buffer[len - 1];
}

uint8_t CM1106SLNSComponent::calculate_checksum_(const uint8_t *buffer, size_t len) {
  uint8_t sum = 0;
  for (size_t i = 0; i < len; i++) {
    sum += buffer[i];
  }
  return (~sum) + 1;
}

// Validate frame header matches expected format: 0x16 0x05
// Reference: UART_COMMUNICATION.md - Estructura de Frames
bool CM1106SLNSComponent::validate_frame_header_(const uint8_t *buffer, size_t len) {
  if (len < 3) return false;  // Need at least header (2 bytes) + command byte
  if (buffer[0] != this->FRAME_HEADER_1) {
    if (this->debug_uart_)
      ESP_LOGW(TAG, "Invalid frame header byte 0: expected 0x%02X got 0x%02X", 
               this->FRAME_HEADER_1, buffer[0]);
    return false;
  }
  if (buffer[1] != this->FRAME_HEADER_2) {
    if (this->debug_uart_)
      ESP_LOGW(TAG, "Invalid frame header byte 1: expected 0x%02X got 0x%02X", 
               this->FRAME_HEADER_2, buffer[1]);
    return false;
  }
  if (buffer[2] != this->FRAME_COMMAND) {
    if (this->debug_uart_)
      ESP_LOGW(TAG, "Invalid frame command byte: expected 0x%02X got 0x%02X", 
               this->FRAME_COMMAND, buffer[2]);
    return false;
  }
  return true;
}

void CM1106SLNSComponent::publish_iaq_(uint16_t co2) {
  // Indoor Air Quality (IAQ) classification based on CO2 levels
  // Using standard IAQ classifications:
  // 1=Excellent (<600), 2=Good (600-800), 3=Acceptable (800-1000),
  // 4=Poor (1000-1500), 5=Very Poor (>1500) ppm
  
  int iaq = 0;
  std::string label;

  if (co2 < 600) {
    iaq = 1;
    label = "Excelente";
  } else if (co2 < 800) {
    iaq = 2;
    label = "Buena";
  } else if (co2 < 1000) {
    iaq = 3;
    label = "Aceptable";
  } else if (co2 < 1500) {
    iaq = 4;
    label = "Mala";
  } else {
    iaq = 5;
    label = "Muy mala";
  }

  if (this->iaq_numeric_ != nullptr)
    this->iaq_numeric_->publish_state(iaq);
  // if (this->iaq_text_ != nullptr)
  //   this->iaq_text_->publish_state(label);
}

void CM1106SLNSComponent::soft_reset_() {
  // Soft reset command: [0x11][0x03][0x02][0x00][0xED]
  // Reference: UART_COMMUNICATION.md - Reset Suave del Sensor
  // This command resets the sensor to its initial state
  
  ESP_LOGW(TAG, ">>> SOFT RESET: Sending reset command [0x11][0x03][0x02][0x00][0xED]");
  const uint8_t reset_cmd[this->RESET_CMD_LENGTH] = {0x11, 0x03, 0x02, 0x00, 0xED};
  this->write_array(reset_cmd, this->RESET_CMD_LENGTH);
  
  // Clear any pending data in the buffer
  while (this->available() >= 1) {
    this->read();
  }
  
  // Reset state variables
  this->awaiting_config_response_ = true;
  this->config_command_sent_ = false;
  this->config_retry_count_ = 0;
  this->config_cmd_time_ = millis();
  this->config_retry_time_ = millis();
  this->last_frame_time_ = millis();
}

// Handle configuration retry logic
// Similar to Arduino setupCM1106() which retries get_working_status() / set_working_status()
// Reference: Arduino my_cm1106.ino lines 74-94
void CM1106SLNSComponent::check_config_retry_() {
  // Timeout waiting for config response
  ESP_LOGW(TAG, "✗ CONFIG RESPONSE TIMEOUT (>%ums, no data), attempt %u/%u", 
           this->CONFIG_RESPONSE_TIMEOUT, this->config_retry_count_ + 1, this->MAX_CONFIG_RETRIES);
  
  this->config_retry_count_++;
  
  if (this->config_retry_count_ < this->MAX_CONFIG_RETRIES) {
    // Retry: Wait a bit then resend config
    if (millis() - this->config_retry_time_ > this->CONFIG_RETRY_DELAY) {
      ESP_LOGW(TAG, "Retrying config command (attempt %u/%u)...", 
               this->config_retry_count_ + 1, this->MAX_CONFIG_RETRIES);
      
      // Clear buffer before retry
      while (this->available() >= 1) {
        this->read();
      }
      
      this->config_cmd_time_ = millis();
      this->config_retry_time_ = millis();
      this->send_config_command_();
    }
  } else {
    // Max retries exceeded - give up and start receiving data anyway
    // The sensor might be in continuous mode already
    ESP_LOGW(TAG, "Max config retries (%u) exceeded. Starting to receive data anyway...", 
             this->MAX_CONFIG_RETRIES);
    ESP_LOGW(TAG, "Sensor may already be in continuous mode or communication is one-way");
    
    this->awaiting_config_response_ = false;
    this->continuous_mode_confirmed_ = false;  // Mark as not confirmed but continuing
    this->config_retry_count_ = 0;
    this->last_frame_time_ = millis();
  }
}

void CM1106SLNSComponent::loop() {
  uint8_t buffer[8];

  // Phase 1: Wait for configuration response
  // Similar to Arduino: setupCM1106() waits for sensor confirmation
  // Reference: UART_COMMUNICATION.md - Secuencia de Inicialización, Arduino my_cm1106.ino
  if (this->awaiting_config_response_) {
    // After soft reset, wait ~1s before sending config again.
    if (!this->config_command_sent_) {
      if (millis() - this->config_cmd_time_ >= this->CONFIG_RETRY_DELAY) {
        ESP_LOGW(TAG, "Step 1: Sending config command after reset delay");
        this->send_config_command_();
        this->config_command_sent_ = true;
        this->config_cmd_time_ = millis();
      }
      return;
    }

    // Log once at the start that we're waiting
    static bool logged_waiting = false;
    if (!logged_waiting) {
      ESP_LOGW(TAG, "Step 2: Waiting for config ACK from sensor (%ums timeout, attempt %u/%u)", 
               this->CONFIG_RESPONSE_TIMEOUT, this->config_retry_count_ + 1, this->MAX_CONFIG_RETRIES);
      logged_waiting = true;
    }
    
    if (this->available() >= this->CONFIG_RESPONSE_LENGTH) {
      uint8_t response[4];
      for (int i = 0; i < this->CONFIG_RESPONSE_LENGTH; i++) {
        response[i] = this->read();
      }
      
      if (this->validate_config_response_(response, this->CONFIG_RESPONSE_LENGTH)) {
        ESP_LOGI(TAG, "✓ CONFIG ACK RECEIVED: 0x%02X 0x%02X 0x%02X 0x%02X", 
                 response[0], response[1], response[2], response[3]);
        ESP_LOGI(TAG, "Step 3: Continuous mode configured successfully");
        ESP_LOGI(TAG, "Step 4: Ready to receive sensor data");
        this->awaiting_config_response_ = false;
        this->continuous_mode_confirmed_ = true;
        this->config_retry_count_ = 0;
        logged_waiting = false;
        this->last_frame_time_ = millis();  // Reset timing for data frames
      } else {
        ESP_LOGW(TAG, "✗ INVALID CONFIG RESPONSE: 0x%02X 0x%02X 0x%02X 0x%02X", 
                 response[0], response[1], response[2], response[3]);
        // Clear UART buffer on invalid response
        while (this->available() >= 1) {
          this->read();
        }
      }
    } else if (millis() - this->config_cmd_time_ > this->CONFIG_RESPONSE_TIMEOUT) {
      // Timeout waiting for config response
      this->check_config_retry_();
      logged_waiting = false;
    }
    return;  // Don't process sensor data until config is done
  }
  
  // Phase 2: Monitor for data timeout
  // Reference: UART_COMMUNICATION.md - Manejo de Errores
  if (millis() - this->last_frame_time_ > (this->measurement_period_ + this->DATA_TIMEOUT_MARGIN)) {
    if (!this->timeout_active_) {
      ESP_LOGW(TAG, "Data Timeout: no frame received for >%ums (expected every %ums)", 
               millis() - this->last_frame_time_, this->measurement_period_ + this->DATA_TIMEOUT_MARGIN);
      if (this->error_sensor_ != nullptr)
        this->error_sensor_->publish_state(true);
      this->timeout_active_ = true;
      this->last_frame_time_ = millis();
    }
  } else {
    if (this->timeout_active_) {
      this->timeout_active_ = false;
    }
  }

  // Phase 3: Read and process data frames
  // Reference: UART_COMMUNICATION.md - Estructura de Frames, Arduino my_cm1106.ino readCM1106()
  // Frame format: [0x16][0x05][0x50][CO2H][CO2L][DF3][DF4][CS]
  while (this->available() >= this->FRAME_LENGTH) {
    for (int i = 0; i < this->FRAME_LENGTH; i++)
      buffer[i] = this->read();

    if (this->debug_uart_)
      ESP_LOGD(TAG, "UART frame: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
               buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);

    this->last_frame_time_ = millis();
    if (this->timeout_active_) {
      this->timeout_active_ = false;
    }
    if (this->error_sensor_ != nullptr)
      this->error_sensor_->publish_state(false);

    // Validate frame header (0x16 0x05 0x50)
    if (!this->validate_frame_header_(buffer, this->FRAME_LENGTH)) {
      this->bad_frames_++;
      if (this->debug_uart_) {
        ESP_LOGW(TAG, "Invalid frame header - bytes: 0x%02X 0x%02X 0x%02X", 
                 buffer[0], buffer[1], buffer[2]);
      }
      if (this->bad_frames_ > this->MAX_BAD_FRAMES) {
        ESP_LOGW(TAG, "Too many bad frames (%u), resetting sensor", this->bad_frames_);
        this->soft_reset_();
        this->bad_frames_ = 0;
      }
      continue;
    }

    // Validate checksum (two's complement of sum of first 7 bytes)
    if (!this->validate_checksum_(buffer, this->FRAME_LENGTH)) {
      // Log checksum details for debugging
      if (this->debug_uart_) {
        uint8_t sum = 0;
        for (size_t i = 0; i < 7; i++) {
          sum += buffer[i];
        }
        uint8_t expected = (~sum) + 1;
        ESP_LOGW(TAG, "Invalid checksum: expected 0x%02X got 0x%02X (sum 0x%02X)", 
                 expected, buffer[7], sum);
      }

      this->bad_frames_++;
      if (this->bad_frames_ > this->MAX_BAD_FRAMES) {
        ESP_LOGW(TAG, "Too many bad checksums (%u), resetting sensor", this->bad_frames_);
        this->soft_reset_();
        this->bad_frames_ = 0;
      }
      continue;
    }

    this->bad_frames_ = 0;

    // Extract CO2 value (bytes 3-4, big-endian)
    // Reference: UART_COMMUNICATION.md - Extracción de Datos, Arduino sensor.co2 = sensor_CM1106->get_co2()
    uint16_t co2 = (buffer[3] << 8) | buffer[4];
    uint8_t df3 = buffer[5];
    uint8_t df4 = buffer[6];

    if (this->df3_sensor_ != nullptr)
      this->df3_sensor_->publish_state(df3);
    if (this->df4_sensor_ != nullptr)
      this->df4_sensor_->publish_state(df4);

    auto status = this->interpret_status_(df3, df4);
    // if (this->status_sensor_ != nullptr)
    //   this->status_sensor_->publish_state(status);

    if (this->debug_uart_)
      ESP_LOGD(TAG, "Parsed: CO2=%u ppm | DF3=0x%02X DF4=0x%02X | Status=%s", 
               co2, df3, df4, status.c_str());

    // Handle warming up state
    // Reference: UART_COMMUNICATION.md - Estados del Sensor (DF3), Arduino if (status == "Warming up")
    if (df3 == this->WARMUP_STATUS_VALUE) {  // 0x08
      if (this->ready_sensor_ != nullptr)
        this->ready_sensor_->publish_state(false);

      if (this->warmup_start_ == 0)
        this->warmup_start_ = millis();

      if (millis() - this->warmup_start_ > this->warmup_timeout_) {
        ESP_LOGW(TAG, "Warmup timeout exceeded (%ums), resetting sensor", this->warmup_timeout_);
        this->soft_reset_();
        this->warmup_start_ = millis();
      }
      continue;
    }

    this->warmup_start_ = 0;
    if (this->ready_sensor_ != nullptr)
      this->ready_sensor_->publish_state(true);

    // Validate CO2 range
    // Reference: UART_COMMUNICATION.md - Validación de Rango CO2
    if (co2 == 0 || co2 < this->CO2_MIN_VALID || co2 > this->CO2_MAX_VALID) {
      if (this->debug_uart_) {
        ESP_LOGD(TAG, "CO2 out of valid range: %u ppm (valid: %u-%u)", 
                 co2, this->CO2_MIN_VALID, this->CO2_MAX_VALID);
      }
      continue;
    }

    // Calculate stability counter
    // Reference: UART_COMMUNICATION.md - Comparativa con Arduino
    if (std::abs((int)co2 - (int)this->last_valid_co2_) < this->STABILITY_THRESHOLD)
      this->stability_counter_ = std::min<uint8_t>(100, this->stability_counter_ + 1);
    else
      this->stability_counter_ = (this->stability_counter_ > 2) ? this->stability_counter_ - 2 : 0;

    if (this->stability_sensor_ != nullptr)
      this->stability_sensor_->publish_state(this->stability_counter_);

    this->last_valid_co2_ = co2;

    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(co2);

    this->publish_iaq_(co2);
  }
}

void CM1106SLNSComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "CM1106SL-NS CO2 Sensor:");
  ESP_LOGCONFIG(TAG, "  Protocol: UART at 9600 bps (see UART_COMMUNICATION.md)");
  ESP_LOGCONFIG(TAG, "  Mode: Continuous (Arduino-compatible implementation)");
  ESP_LOGCONFIG(TAG, "  Status: %s", 
                this->continuous_mode_confirmed_ ? "Configured ✓" : "Pending");
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "DF3 (Status byte)", this->df3_sensor_);
  LOG_SENSOR("  ", "DF4 (Info byte)", this->df4_sensor_);
  LOG_SENSOR("  ", "Stability", this->stability_sensor_);
  LOG_BINARY_SENSOR("  ", "Ready", this->ready_sensor_);
  LOG_BINARY_SENSOR("  ", "Error", this->error_sensor_);
  LOG_SENSOR("  ", "IAQ Numeric", this->iaq_numeric_);
  ESP_LOGCONFIG(TAG, "  UART Debug: %s", this->debug_uart_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  Config: period=%us, smoothing=%u", 
                this->config_period_s_, this->smoothing_samples_);
  ESP_LOGCONFIG(TAG, "  Measurement period: %ums", this->measurement_period_);
  ESP_LOGCONFIG(TAG, "  Warmup timeout: %ums", this->warmup_timeout_);
  ESP_LOGCONFIG(TAG, "  Config retry: max %u attempts, delay %ums between retries", 
                this->MAX_CONFIG_RETRIES, this->CONFIG_RETRY_DELAY);
  ESP_LOGCONFIG(TAG, "  Data timeout margin: %ums", this->DATA_TIMEOUT_MARGIN);
  ESP_LOGCONFIG(TAG, "  CO2 valid range: %u - %u ppm", this->CO2_MIN_VALID, this->CO2_MAX_VALID);
  ESP_LOGCONFIG(TAG, "  Stability threshold: %u ppm", this->STABILITY_THRESHOLD);
  ESP_LOGCONFIG(TAG, "  Expected UART baud_rate: 9600");
  this->check_uart_settings(9600);
}

void CM1106SLNSComponent::send_config_command_() {
  // Command format: [0x11][0x04][0x50][DF1][DF2][DF3][CS]
  // Reference: UART_COMMUNICATION.md - Comando de Configuración
  // Arduino: sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT)
  // This command sets the sensor to continuous mode with specified period and smoothing
  //
  // DF1 = Period_s / 256 (MSB of period in seconds)
  // DF2 = Period_s % 256 (LSB of period in seconds)
  // DF3 = Smoothing samples (1-255)
  // CS = Checksum (two's complement)
  
  uint8_t df1 = this->config_period_s_ / 256;
  uint8_t df2 = this->config_period_s_ % 256;
  uint8_t df3 = this->smoothing_samples_;
  
  uint8_t cmd[7] = {0x11, 0x04, 0x50, df1, df2, df3, 0x00};
  cmd[6] = this->calculate_checksum_(cmd, 6);
  
  ESP_LOGW(TAG, ">>> SENDING CONFIG COMMAND");
  ESP_LOGW(TAG, "    Period: %u seconds (0x%02X 0x%02X)", 
           this->config_period_s_, df1, df2);
  ESP_LOGW(TAG, "    Smoothing: %u samples", this->smoothing_samples_);
  ESP_LOGD(TAG, "    Bytes: 0x11 0x04 0x50 0x%02X 0x%02X 0x%02X 0x%02X",
           df1, df2, df3, cmd[6]);
  
  this->write_array(cmd, 7);
}

bool CM1106SLNSComponent::validate_config_response_(const uint8_t *buffer, size_t len) {
  // Expected response: [0x16][0x01][0x50][CS]
  // Reference: UART_COMMUNICATION.md - Respuesta del Sensor
  // Arduino: validates the response from sensor_CM1106->set_working_status()
  
  if (len != this->CONFIG_RESPONSE_LENGTH) {
    if (this->debug_uart_)
      ESP_LOGW(TAG, "Config response wrong length: expected %u got %u", 
               this->CONFIG_RESPONSE_LENGTH, len);
    return false;
  }
  if (buffer[0] != this->CONFIG_RESPONSE_BYTE_1) {
    if (this->debug_uart_)
      ESP_LOGW(TAG, "Config response byte[0]: expected 0x%02X got 0x%02X", 
               this->CONFIG_RESPONSE_BYTE_1, buffer[0]);
    return false;
  }
  if (buffer[1] != this->CONFIG_RESPONSE_BYTE_2) {
    if (this->debug_uart_)
      ESP_LOGW(TAG, "Config response byte[1]: expected 0x%02X got 0x%02X", 
               this->CONFIG_RESPONSE_BYTE_2, buffer[1]);
    return false;
  }
  if (buffer[2] != this->CONFIG_RESPONSE_CMD) {
    if (this->debug_uart_)
      ESP_LOGW(TAG, "Config response cmd byte: expected 0x%02X got 0x%02X", 
               this->CONFIG_RESPONSE_CMD, buffer[2]);
    return false;
  }
  
  return this->validate_checksum_(buffer, len);
}

}  // namespace cm1106sl_ns
}  // namespace esphome
