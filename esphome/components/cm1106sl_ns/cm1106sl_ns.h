#include "esphome.h"

class CM1106SLNS : public esphome::PollingComponent, public esphome::uart::UARTDevice {
 public:
  esphome::sensor::Sensor *co2_sensor;
  esphome::sensor::Sensor *df3_sensor;
  esphome::sensor::Sensor *df4_sensor;
  esphome::text_sensor::TextSensor *status_sensor;
  esphome::sensor::Sensor *stability_sensor;
  esphome::binary_sensor::BinarySensor *ready_sensor;
  esphome::binary_sensor::BinarySensor *error_sensor;
  esphome::sensor::Sensor *iaq_numeric;
  esphome::text_sensor::TextSensor *iaq_text;

  uint16_t last_valid_co2 = 0;
  uint8_t stability_counter = 0;
  uint32_t last_frame_time = 0;
  uint32_t warmup_start = 0;
  uint8_t bad_frames = 0;

  CM1106SLNS(esphome::uart::UARTComponent *parent) : esphome::uart::UARTDevice(parent) {}

  std::string interpret_status(uint8_t df3, uint8_t df4) {
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

  bool validate_checksum(uint8_t *buffer) {
    uint8_t sum = 0;
    for (int i = 0; i < 7; i++)
      sum += buffer[i];
    sum = (~sum) + 1;
    return sum == buffer[7];
  }

  void publish_iaq(uint16_t co2) {
    int iaq = 0;
    std::string label;

    if (co2 < 600) { iaq = 1; label = "Excelente"; }
    else if (co2 < 800) { iaq = 2; label = "Buena"; }
    else if (co2 < 1000) { iaq = 3; label = "Aceptable"; }
    else if (co2 < 1500) { iaq = 4; label = "Mala"; }
    else { iaq = 5; label = "Muy mala"; }

    if (iaq_numeric != nullptr)
      iaq_numeric->publish_state(iaq);
    if (iaq_text != nullptr)
      iaq_text->publish_state(label);
  }

  void soft_reset() {
    ESP_LOGW("cm1106sl", "Soft reset del sensor");
    const uint8_t reset_cmd[5] = {0x11, 0x03, 0x02, 0x00, 0xED};
    this->write_array(reset_cmd, 5);
  }

  void restart_uart() {
    ESP_LOGW("cm1106sl", "Reiniciando UART por bloqueo");
    this->parent_->flush();
  }

  void update() override {
    uint8_t buffer[8];

    if (millis() - last_frame_time > 15000) {
      if (error_sensor != nullptr)
        error_sensor->publish_state(true);
      restart_uart();
    }

    while (this->available() >= 8) {
      for (int i = 0; i < 8; i++)
        buffer[i] = this->read();

      last_frame_time = millis();
      if (error_sensor != nullptr)
        error_sensor->publish_state(false);

      if (!validate_checksum(buffer)) {
        bad_frames++;
        if (bad_frames > 5) {
          restart_uart();
          bad_frames = 0;
        }
        continue;
      }

      bad_frames = 0;

      uint16_t co2 = (buffer[3] << 8) | buffer[4];
      uint8_t df3 = buffer[5];
      uint8_t df4 = buffer[6];

      if (df3_sensor != nullptr)
        df3_sensor->publish_state(df3);
      if (df4_sensor != nullptr)
        df4_sensor->publish_state(df4);

      auto status = interpret_status(df3, df4);
      if (status_sensor != nullptr)
        status_sensor->publish_state(status);

      if (status == "Warming up") {
        if (ready_sensor != nullptr)
          ready_sensor->publish_state(false);

        if (warmup_start == 0)
          warmup_start = millis();

        if (millis() - warmup_start > 60000) {
          soft_reset();
          warmup_start = millis();
        }
        continue;
      }

      warmup_start = 0;
      if (ready_sensor != nullptr)
        ready_sensor->publish_state(true);

      if (co2 == 0 || co2 < 300 || co2 > 5000)
        continue;

      if (std::abs((int)co2 - (int)last_valid_co2) < 20)
        stability_counter = std::min<uint8_t>(100, stability_counter + 1);
      else
        stability_counter = (stability_counter > 2) ? stability_counter - 2 : 0;

      if (stability_sensor != nullptr)
        stability_sensor->publish_state(stability_counter);

      last_valid_co2 = co2;

      if (co2_sensor != nullptr)
        co2_sensor->publish_state(co2);

      publish_iaq(co2);
    }
  }
};
