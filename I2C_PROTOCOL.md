# CM1106SL-NS I2C Protocol Implementation

## Overview
The CM1106SL-NS is a super low-power NDIR CO2 sensor module that supports I2C communication in two measurement modes:

- **Single measurement mode / working mode A**: default mode. The host starts each measurement explicitly.
- **Continuous measurement mode / working mode B**: the sensor measures automatically at a configured interval.

This document is based on the CM1106SL-NS datasheet V0.7 IIC communication protocol.

## I2C Configuration

### Basic Settings
- **I2C Address**: 0x34 (7-bit address, default from datasheet)
- **Write Address**: 0x68 (0x34 << 1 | 0)
- **Read Address**: 0x69 (0x34 << 1 | 1)
- **Master/slave mode**: Sensor is always the slave
- **Data Rate**: Up to 100 kbit/s (standard mode)
- **Clock Stretching**: Supported; the sensor can hold SCL low while it evaluates a received byte
- **Pull-up Resistors**: 10kΩ (SCL and SDA)
- **SCL Frequency**: 10kHz - 100kHz
- **EEPROM Write Time**: Less than 25ms for EEPROM-mapped registers

## Operating Modes

### Single Measurement Mode (Mode A)
Single measurement mode is the default mode. The sensor waits for the host to start each measurement.

**Configuration:**
- Register 0x95 (Measurement Mode): Set to 0x01 for single mode
- Register 0x93 (Start Single Measurement): Write 0x01 to start one measurement
- Register 0x96/0x97 (Measurement Period): Not used in single mode

**Cycle:**
1. Host ensures the sensor is in single measurement mode (0x95 = 0x01)
2. Host writes 0x01 to register 0x93 to start a measurement
3. Host waits until the measurement is ready
4. Host reads CO2 from registers 0x06/0x07

The RDY pin indicates that sensor data is ready and the host can communicate. RDY is active low.

### Continuous Measurement Mode (Mode B)
In continuous mode, the sensor automatically measures CO2 at regular intervals without requiring a trigger command from the host.

**Configuration:**
- Register 0x95 (Measurement Mode): Set to 0x00 for continuous mode
- Register 0x96/0x97 (Measurement Period): Set measurement interval in seconds (2-65534s)
- Odd measurement period values are rounded up to the nearest even value
- Default measurement cycle: 2 minutes (120 seconds)

**Cycle:**
1. Host configures continuous measurement mode (0x95 = 0x00)
2. Optionally, host configures the measurement period in 0x96/0x97
3. Sensor performs measurements automatically and updates registers 0x06/0x07
4. Host reads CO2 from registers 0x06/0x07 after each measurement cycle

## I2C Registers

### Read-Only Registers

| Register | Name | Description |
|----------|------|-------------|
| 0x00 | Reserved | Reserved |
| 0x01 | Error Status | Bit 0 = fatal error, bit 5 = out of range detection |
| 0x06/0x07 | CO2 Concentration | High/Low bytes, CO2 = (0x06 * 256) + 0x07 (ppm) |
| 0x08/0x09 | Temperature | High/Low bytes, T = (0x08 * 256 + 0x09) / 100 (°C) |
| 0x0D | Measurement Count | Counter incremented after each measurement (0-255, wraps around) |
| 0x0E/0x0F | Measurement Cycle Time | Current time in the present measurement cycle, incremented every 2s |
| 0x10/0x11 | CO2 Concentration | Additional CO2 concentration register pair |
| 0x12/0x13 | CO2 Concentration | Additional CO2 concentration register pair |
| 0x14/0x15 | CO2 Concentration | Additional CO2 concentration register pair |

### Read/Write Registers

| Register | Name | Range/Values | Default |
|----------|------|---|---------|
| 0x81 | Calibration Status | Read-only calibration status bits | - |
| 0x82/0x83 | Calibration Command | 0x7C05=target, 0x7C06=background, 0x7C07=zero | - |
| 0x84/0x85 | Calibration Target | Target value used by target calibration | 400ppm |
| 0x86/0x87 | CO2 Value Override | 32767 means no override | 32767 |
| 0x88/0x89 | ABC Time | Time since last ABC calibration in half-hour units | - |
| 0x93 | Start Single Measurement | Write 0x01 to start one single-mode measurement | - |
| 0x95 | Measurement Mode (EE) | 0x00=continuous, 0x01=single | 0x01 |
| 0x96/0x97 | Measurement Period (EE) | 2-65534 seconds, continuous mode only | 120s (2min) |
| 0x9A/0x9B | ABC Period (EE) | 24-240 hours | 168h (7 days) |
| 0x9E/0x9F | ABC Target (EE) | Target value for background and ABC calibration | 400ppm |
| 0xA5 | Meter Control | Bit 1: 0=ABC enabled, 1=ABC disabled | ABC disabled |
| 0xA7 | I2C Address (EE) | 1-0x7F | 0x34 |

Registers marked `(EE)` are EEPROM-mapped. Avoid removing power while an EEPROM write is in progress.

## Data Reading

### Reading CO2 Concentration

```
Master → Slave: START + 0x68 + 0x06 (register address) + STOP
Master → Slave: START + 0x69 (read address)
Slave → Master: [HIGH_BYTE] [LOW_BYTE] + NACK + STOP

CO2 (ppm) = (HIGH_BYTE << 8) | LOW_BYTE
```

Datasheet example:

```
Response: 04 EE
CO2 = 0x04EE = 1262 ppm
```

### Continuous Measurement Cycle

1. Master polls the sensor at intervals defined by measurement period
2. Sensor performs measurement and updates registers 0x06/0x07
3. Master reads CO2 value from registers 0x06/0x07 via I2C
4. Cycle repeats

### Single Measurement Cycle

```
Master -> Slave: START + 0x68 + 0x95 (register) + 0x01 (single mode) + STOP
Master -> Slave: START + 0x68 + 0x93 (register) + 0x01 (start one measurement) + STOP
Wait until measurement is ready
Master reads CO2 concentration from 0x06/0x07
```

If the RDY pin is connected, wait until RDY goes low before reading. If RDY is not connected, the host must wait long enough for one measurement to finish before reading.

## Configuration via I2C

### Set Continuous Measurement Mode

```
Master → Slave: START + 0x68 + 0x95 (register) + 0x00 (value) + STOP
```

### Set Single Measurement Mode

```
Master → Slave: START + 0x68 + 0x95 (register) + 0x01 (value) + STOP
```

### Start Single Measurement

```
Master → Slave: START + 0x68 + 0x93 (register) + 0x01 (value) + STOP
```

### Set Measurement Period

```
Master → Slave: START + 0x68 + 0x96 (register) + 0xXX (high byte) + 0xXX (low byte) + STOP

Period (seconds) = (high_byte << 8) | low_byte
```

This setting is only used in continuous measurement mode. Odd values are rounded up to the nearest even value by the sensor.

### Example: Set 60-second measurement period
```
Period = 60 = 0x003C
Master → Slave: START + 0x68 + 0x96 + 0x00 + 0x3C + STOP
```

### Datasheet Cubic Test Board Examples

The datasheet examples include extra bytes used by the Cubic testing board command format. For example:

```
Read result:                 68 06 00 02, then 69 -> [HIGH_BYTE] [LOW_BYTE]
Set continuous working mode: 68 95 00 01 01
```

For ESPHome's I2C register helpers, this maps to writing the register address and value(s), e.g. `0x95, 0x00` for continuous mode.

## ESP32 Implementation (ESPHome)

### Wiring
```
CM1106SL-NS  →  ESP32
GND          →  GND
VBB (3.3-5.5V) → VCC
RX/SDA       →  GPIO21 (I2C SDA)
TX/SCL       →  GPIO22 (I2C SCL)
VDDIO        →  3.3V
EN           →  VCC in continuous mode; can be host-controlled in low-power single mode
RDY          →  Optional ready signal, active low
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
    measurement_period: 60s  # Host read period; continuous sensor period must also be written to 0x96/0x97
    debug: false             # Enable I2C debug logging
    
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

1. **Default mode**: The sensor defaults to single measurement mode (0x95 = 0x01).
2. **Continuous mode**: Set 0x95 = 0x00. The sensor measures automatically. Set 0x96/0x97 if the internal measurement period must differ from the default 120s.
3. **Single mode**: Set 0x95 = 0x01. Write 0x01 to 0x93 for every measurement, then wait for RDY low or another suitable delay before reading 0x06/0x07.
4. **Reading**: CO2 is read from registers 0x06/0x07.
5. **Accuracy**: Single measurement output has no moving average. Continuous mode can use moving average behavior for better stability.
6. **Measurement Range**: 0-5000 ppm.
7. **Warm-up**: Sensor may take several seconds to stabilize on first power-up.
8. **Auto-Calibration**: Can be enabled/disabled via register 0xA5 (ABC).

## Error Handling

- **Invalid CO2 values**: Values outside the useful sensor range should be rejected by the host application.
- **I2C read failures**: Treat as communication errors and retry on the next measurement cycle.
- **Fatal error**: Error status bit 0 indicates analog front-end initialization failure; power-cycle the sensor.
- **Out of range**: Error status bit 5 indicates the measured concentration is outside the sensor range.

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
