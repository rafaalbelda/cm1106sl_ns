# cm1106sl_ns

CM1106SL-NS CO2 sensor component for ESPHome using the command-oriented I2C protocol from the `cm1106_i2s/cm1106_i2c` library.

Default I2C address: `0x31`.

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22
  frequency: 100kHz

sensor:
  - platform: cm1106sl_ns
    measurement_period: 60s
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

Status values follow the reference library:

- `0`: preheating
- `1`: normal operation
- `2`: operating trouble
- `3`: out of full scale
- `5`: non-calibrated
