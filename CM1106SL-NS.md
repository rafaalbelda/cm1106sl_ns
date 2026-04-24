# CM1106SL-NS Sensor Information

## UART Communication Protocol

### Read Measured Result of CO2
- **Command:** `11 01 01 ED`
- **Response:** `16 05 01 DF1 DF2 DF3 DF4 [CS]`
- **Calculation:** CO2 concentration = `DF1 * 256 + DF2`

### ABC Parameter Check
- **Command:** `11 01 0F DF`
- **Response:** `16 07 0F [DF1] [DF2] [DF3] [DF4] [DF5] [DF6] [CS]`
- **Explanation:**
  - `DF1`: Reserved (default 100 / 0x64)
  - `DF2`: Open/close auto calibration (0: open; 2: close, default is close)
  - `DF3`: Calibration cycle (1-10 optional, 7 days is default)
  - `DF4` and `DF5`: High and low baseline of calibration (baseline = `DF4 * 256 + DF5`)
  - `DF6`: Reserved (default 100 / 0x64)

### Set ABC Parameter
- **Command:** `11 07 10 [DF1][DF2][DF3][DF4][DF5][DF6][CS]`
- **Response:** `16 01 10 D9`

### Read Software Version
- **Command:** `11 01 1E D0`

### Set/Check Working Status
- **Set Command:** `11 02 51 [DF1] [CS]`
- **Check Command:** `11 01 51 [CS]`
- **Description:** `DF1 = 0` (single measurement mode), `DF1 = 1` (continuous measurement mode)
