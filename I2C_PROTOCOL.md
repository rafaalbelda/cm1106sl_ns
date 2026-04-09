# CM1106SL-NS I2C Protocol Implementation

## Overview
The CM1106SL-NS is a super low-power NDIR CO2 sensor module that supports I2C communication in **continuous measurement mode**.

## I2C Configuration

### Basic Settings
- **I2C Address**: 0x34 (7-bit address, default from datasheet)
- **Write Address**: 0x68 (0x34 << 1 | 0)
- **Read Address**: 0x69 (0x34 << 1 | 1)
- **Data Rate**: Up to 100 kbit/s (standard mode)
- **Pull-up Resistors**: 10kΩ (SCL and SDA)
- **SCL Frequency**: 10kHz - 100kHz

## Operating Mode

### Continuous Measurement Mode (Mode B)
In continuous mode, the sensor automatically measures CO2 at regular intervals without requiring a trigger command from the host.

**Configuration:**
- Register 0x95 (Measurement Mode): Set to 0x00 for continuous mode
- Register 0x96/0x97 (Measurement Period): Set measurement interval in seconds (2-65534s)
- Default: 2 minutes (120 seconds) measurement cycle

## I2C Registers

### Read-Only Registers

| Register | Name | Description |
|----------|------|-------------|
| 0x00 | Error Status | Bit 5 = Out of range detection |
| 0x06/0x07 | CO2 Concentration | High/Low bytes, CO2 = (0x06 × 256) + 0x07 (ppm) |
| 0x08/0x09 | Temperature | High/Low bytes, T = (0x08 × 256 + 0x09) / 100 (°C) |
| 0x0D | Measurement Count | Counter incremented after each measurement (0-255) |
| 0x0E/0x0F | Cycle Time | Incremented every 2s during measurement cycle |

### Read/Write Registers

| Register | Name | Range/Values | Default |
|----------|------|---|---------|
| 0x95 | Measurement Mode | 0x00=continuous, 0x01=single | 0x01 |
| 0x96/0x97 | Measurement Period | 2-65534 seconds | 120s (2min) |
| 0x9A/0x9B | ABC Period | 24-240 hours | 168h (7 days) |
| 0x9E/0x9F | ABC Target | 400ppm (default) | 400ppm |
| 0xA5 | Meter Control | Bit 1: 0=ABC enabled, 1=ABC disabled | ABC disabled |
| 0xA7 | I2C Address | 1-0x7F (EEPROM) | 0x34 |

## Data Reading

### Reading CO2 Concentration

```
Master → Slave: START + 0x68 + 0x06 (register address) + STOP
Master → Slave: START + 0x69 (read address)
Slave → Master: [HIGH_BYTE] [LOW_BYTE] + NACK + STOP

CO2 (ppm) = (HIGH_BYTE << 8) | LOW_BYTE
```

### Measurement Cycle

1. Master polls the sensor at intervals defined by measurement period
2. Sensor performs measurement and updates registers 0x06/0x07
3. Master reads CO2 value from registers 0x06/0x07 via I2C
4. Cycle repeats

## Configuration via I2C

### Set Continuous Measurement Mode

```
Master → Slave: START + 0x68 + 0x95 (register) + 0x00 (value) + STOP
```

### Set Measurement Period

```
Master → Slave: START + 0x68 + 0x96 (register) + 0xXX (high byte) + 0xXX (low byte) + STOP

Period (seconds) = (high_byte << 8) | low_byte
```

### Example: Set 60-second measurement period
```
Period = 60 = 0x003C
Master → Slave: START + 0x68 + 0x96 + 0x00 + 0x3C + STOP
```

## ESP32 Implementation (ESPHome)

### Wiring
```
CM1106SL-NS  →  ESP32
GND          →  GND
VBB (3.3-5.5V) → VCC
RX/SDA       →  GPIO21 (I2C SDA)
TX/SCL       →  GPIO22 (I2C SCL)
VDDIO        →  3.3V
EN           →  VCC (always enabled)
RDY          →  Optional (not used)
COMSEL       →  GND (I2C mode)
```

### YAML Configuration
```yaml
i2c:
  sda: GPIO21
  scl: GPIO22

sensor:
  - platform: cm1106sl_ns
    id: my_co2
    update_interval: 60s  # Measurement period in ESPHome
    debug: false          # Enable I2C debug logging
    
    co2:
      name: "CO2 Concentration"
    
    stability:
      name: "CO2 Stability"
    
    ready:
      name: "Sensor Ready"
    
    error:
      name: "Sensor Error"
    
    iaq_numeric:
      name: "Air Quality Index"
```

## Operation Notes

1. **Initialization**: On startup, the sensor is configured for continuous measurement mode with the specified measurement period
2. **Reading**: The ESP32 reads CO2 registers (0x06/0x07) at the `update_interval` rate
3. **Accuracy**: ±(50ppm+5% of reading) in continuous mode with moving average
4. **Measurement Range**: 0-5000 ppm
5. **Warm-up**: Sensor may take several seconds to stabilize on first power-up
6. **Auto-Calibration**: Can be enabled/disabled via register 0xA5 (ABC)

## Error Handling

- **Invalid CO2 values** (0, <300, >5000 ppm): Tracked with `bad_frames_` counter; soft reset after 5 consecutive errors
- **I2C read failures**: Sensor error flag set; automatic retry on next measurement cycle
- **Out of range**: Detector in register 0x00, bit 5

## Temperature Compensation

Temperature can be read from registers 0x08/0x09 for additional accuracy:
```
Temperature (°C) = (0x08 × 256 + 0x09) / 100
```

## References
- CM1106SL-NS Datasheet V0.7 (November 5, 2021)
- NDIR (Non-Dispersive Infrared) technology
- Manufacturer: Cubic Sensor and Instrument Co., Ltd
- Contact: info@gassensor.com.cn
