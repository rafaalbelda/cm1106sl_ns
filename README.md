# cm1106sl_ns

CM1106SL-NS CO2 sensor component for ESPHome using the I2C register protocol from the datasheet.

Default I2C address: `0x34`.

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22
  frequency: 100kHz

sensor:
  - platform: cm1106sl_ns
    measurement_period: 60s
    measurement_mode: single
    debug: false

    co2:
      name: "CO2"

    status:
      name: "CM1106 Status"

    iaq_numeric:
      name: "CO2 IAQ"

    ready:
      name: "CM1106 Ready"

    error:
      name: "CM1106 Error"
```

Optional outputs under `platform: cm1106sl_ns`: `co2`, `stability`, `status`, `iaq_numeric`, `ready`, and `error`.

Continuous mode can be selected explicitly. `continuous_measurement_period` is written to sensor registers `0x96/0x97`; `measurement_period` is how often ESPHome reads the latest value.

```yaml
sensor:
  - platform: cm1106sl_ns
    measurement_mode: continuous
    continuous_measurement_period: 120s
    measurement_period: 120s
    co2:
      name: "CO2"
```

The optional `status` sensor exposes the raw error status register `0x01`.
