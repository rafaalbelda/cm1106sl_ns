# Análisis Comparativo: Documentación UART vs Implementación Arduino

## Resumen Ejecutivo

✅ **COMPATIBLE:** El código Arduino utiliza correctamente los modos continuo y single descritos en la documentación.

⚠️ **ABSTRACCIÓN:** El código Arduino usa la librería externa `cm1106_uart.h` que oculta los detalles del protocolo UART de bajo nivel.

---

## 1. Configuración Física UART

### Documentación UART
```
Velocidad: 9600 bps
Bits: 8, Sin paridad, 1 bit parada
```

### Implementación Arduino
```c
#define CM1106_BAUDRATE 9600
SoftwareSerial CM1106_serial(CM1106_RX_PIN, CM1106_TX_PIN);
sensor_CM1106 = new CM1106_UART(CM1106_serial);
```

**Verificación:** ✅ Coincide exactamente. Usa 9600 bps como se especifica.

---

## 2. Lectura de Datos (Modo Continuo)

### Documentación UART

**Frame esperado cada Period_s segundos:**
```
[16] [05] [50] [CO2H] [CO2L] [DF3] [DF4] [CS]
```

**Extracción de CO2:**
```c
uint16_t co2_ppm = (buffer[3] << 8) | buffer[4];
```

### Implementación Arduino

```c
sensor.co2 = sensor_CM1106->get_co2();
Serial.printf("CO2 value: %d ppm\n", sensor.co2);
```

**Verificación:** ✅ La librería `cm1106_uart.h` extrae automáticamente el valor CO2 del frame.

**Nivel de Abstracción:**
- Documentación: Muestra bytes raw (0x16, 0x05, 0x50, ...)
- Arduino: Función de alto nivel `get_co2()` retorna `int16_t`

---

## 3. Modo Continuo (Continuous Mode)

### Documentación UART

**Comando de configuración:**
```
[11] [04] [50] [DF1] [DF2] [DF3] [CS]

Parámetros:
- DF1/DF2: Period_s en 16 bits (1-65535 segundos)
- DF3: Muestras suavizado (1-255)

Respuesta esperada:
[16] [01] [50] [CS]
```

### Implementación Arduino

```c
// Detectar modo actual
if (sensor_CM1106->get_working_status(&mode)) {
    if (mode == CM1106_SINGLE_MEASUREMENT) {
        // Cambiar a continuo
        sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT);
    }
}

// Obtener configuración actual
if (sensor_CM1106->get_measurement_period(&period, &smoothed)) {
    DEBUG_OUT.printf(" Period: %d\n", period);
    DEBUG_OUT.printf(" Smoothed: %d\n", smoothed);
}
```

**Verificación:** ✅ Arduino implementa correctamente:
- Lee el modo (continuo/single)
- Cambia a modo continuo si es necesario
- Obtiene período y suavizado

**Diferencias Implementación:**

| Aspecto | Documentación | Arduino |
|--------|---|---|
| **Envío de config** | Bytes raw con checksum manual | Función `set_working_status()` |
| **Lectura respuesta** | Validar 4 bytes: `16 01 50 CS` | Retorno boolean de función |
| **Timeout respuesta** | Implementar timeout 2s manual | Manejado internamente |
| **Período** | Configurar explícitamente | `get_measurement_period()` retorna valor actual |

---

## 4. Modo Single (Single Measurement)

### Documentación UART

**Método:**
```c
// Configurar período máximo (65535 segundos)
uint8_t cmd[7] = {0x11, 0x04, 0x50, 0xFF, 0xFF, 0x01, 0x00};
cmd[6] = calculate_checksum(cmd, 6);
uart_write_bytes(cmd, 7);

// O usar comando de lectura manual (si soporta)
uint8_t single_shot[4] = {0x11, 0x01, 0x50, 0x00};
```

### Implementación Arduino

```c
if (mode == CM1106_SINGLE_MEASUREMENT) {
    DEBUG_OUT.println(" Single");
    // Cambiar a continuo automáticamente
    sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT);
}
```

**Verificación:** ⚠️ Arduino detecta el modo single pero lo cambia a continuo automáticamente.

**Observaciones:**
- Arduino NO mantiene el sensor en modo single
- La librería `set_working_status()` probablemente envía un comando de configuración diferente
- No hay uso de período largo para emular single

---

## 5. Validación y Checksum

### Documentación UART

**Fórmula de checksum:**
```c
uint8_t checksum = (~sum) + 1;  // Complemento a dos
```

**Validación:**
```c
uint8_t expected = (~sum_7_bytes) + 1;
if (expected == buffer[7]) {
    // Frame válido
}
```

### Implementación Arduino

```c
// Internamente en cm1106_uart.h (no visible)
// Se asume que la librería:
// 1. Calcula checksum para comandos enviados
// 2. Valida checksum de respuestas
// 3. Detecta y maneja frames corruptos
```

**Verificación:** ✅ (Asumido) La librería probablemente lo implementa correctamente.

---

## 6. Lectura de Datos en Loop

### Documentación UART

```c
// Esperar período configurado
if (millis() - last_frame_time > measurement_period) {
    // Leer 8 bytes del buffer UART
    while (this->available() >= 8) {
        for (int i = 0; i < 8; i++)
            buffer[i] = this->read();
        
        // Validar checksum
        if (!validate_checksum(buffer, 8)) {
            bad_frames_++;
            continue;
        }
        
        // Procesar CO2
        uint16_t co2 = (buffer[3] << 8) | buffer[4];
    }
}
```

### Implementación Arduino

```c
void readCM1106() {
    sensor.co2 = sensor_CM1106->get_co2();
    Serial.printf("CO2 value: %d ppm\n", sensor.co2);
    
    #ifdef CM_USE_TASKS
        DEBUG_OUT.print("CM1106 >> wait ");
        DEBUG_OUT.print(int(period));
        DEBUG_OUT.println(" seconds before next cycle...");
        DELAY(period*1000);
    #endif
}
```

**Diferencias:**

| Aspecto | Documentación | Arduino |
|--------|---|---|
| **Lectura bytes raw** | Implementar en loop | Delegado a librería |
| **Validación checksum** | Manual explícita | Interno (librería) |
| **Manejo buffer UART** | Lectura continua de 8 bytes | Encapsulado |
| **Timeout datos** | Implementar (>measurement_period) | Probablemente manejado |
| **Espera período** | Condicional con millis() | `DELAY(period*1000)` |

---

## 7. Detección de Versión del Sensor

### Implementación Arduino (No en documentación)

```c
sensor_CM1106->get_software_version(sensor.softver);
if (len >= 10 && !strncmp(sensor.softver+len-5, "SL-NS", 5)) {
    DEBUG_OUT.println("CM1106SL-NS detected");
}
```

**Nota:** La documentación UART no incluye comando para obtener versión de software.

---

## 8. Calibración ABC (Auto Baseline Calibration)

### Implementación Arduino (No en documentación)

```c
if (sensor_CM1106->get_ABC(&abc)) {
    if (abc.open_close == CM1106_ABC_OPEN) {
        DEBUG_OUT.println(" Auto calibration is enabled");
    }
    DEBUG_OUT.printf(" Calibration cycle: %d\n", abc.cycle);
    DEBUG_OUT.printf(" Calibration baseline: %d\n", abc.base);
}
```

**Nota:** La documentación UART no describe estos comandos.

---

## 9. Parámetros de Configuración Real

### Documentación

```
Período: 1-65535 segundos (configurable)
Muestras suavizado: 1-255 (configurable)
```

### Arduino (En ejecución)

```c
if (sensor_CM1106->get_measurement_period(&period, &smoothed)) {
    // Lee valores actuales del sensor
    // Se imprime: período en segundos, muestras suavizado
}
```

**Verificación:** ✅ Arduino obtiene y respeta la configuración existente.

---

## 10. Estados del Sensor (DF3)

### Documentación UART

```c
uint8_t df3 = buffer[5];

if (df3 == 0x08) return "Warming up";
if (df3 == 0x00) return "Normal";
if (df3 == 0x01) return "Sensor error";
if (df3 == 0x02) return "Calibration required";
```

### Implementación Arduino

```c
// No hay parsing explícito de DF3 en el código mostrado
// La librería probablemente la procesa internamente
```

**Verificación:** ⚠️ Arduino no muestra el estado explícitamente. La información de DF3/DF4 no se extrae en el código mostrado.

---

## 11. Manejo de Errores

### Documentación UART

```c
// Timeout de datos
if (millis() - last_frame_time > timeout) {
    error_sensor_->publish_state(true);
}

// Frames corruptos
if (!validate_checksum()) {
    bad_frames_++;
    if (bad_frames_ > 5) soft_reset();
}

// Reset suave
void soft_reset_() {
    const uint8_t reset_cmd[5] = {0x11, 0x03, 0x02, 0x00, 0xED};
}
```

### Implementación Arduino

```c
// No hay manejo explícito de timeouts o reset suave
// Se asume manejo interno de la librería
```

**Verificación:** ⚠️ Arduino no implementa explícitamente el manejo de errores de la documentación.

---

## 12. Detección y Control de Modo de Operación (CRÍTICO)

### Documentación UART
```c
// NO DOCUMENTADO EXPLÍCITAMENTE
// Pero se puede inferir de comandos 0x11 0x04 0x50 vs otros modos
```

### Librería Arduino (cm1106_uart.cpp y cm1106_uart.h)

**Constantes de Modo:**
```c
#define CM1106_CMD_WORKING_STATUS         0x51   // Comando para leer/cambiar modo
#define CM1106_SINGLE_MEASUREMENT          0     // Modo: lectura única (query)
#define CM1106_CONTINUOUS_MEASUREMENT      1     // Modo: envío continuo de frames
```

#### Función: `get_working_status(uint8_t *mode)`

**Propósito:** Leer el modo actual en el que opera el sensor.

**Protocolo Exacto:**
```
Envío (4 bytes):
  [0x11] - Identificador de paquete (CM1106_MSG_IP)
  [0x01] - Longitud (4-3=1)
  [0x51] - Comando (CM1106_CMD_WORKING_STATUS)
  [CS]   - Checksum calculado

Respuesta esperada (5 bytes):
  [0x16] - Identificador ACK (CM1106_MSG_ACK)
  [0x02] - Longitud (5-3=2)
  [0x51] - Comando eco
  [MODE] - 0x00 = Single, 0x01 = Continuous
  [CS]   - Checksum
```

**Código de Librería:**
```c
bool CM1106_UART::get_working_status(uint8_t *mode) {
    bool result = false;

    if (mode == NULL)
        return result;

    // Envía: 0x11 0x01 0x51 [CS]
    send_cmd_data(CM1106_CMD_WORKING_STATUS, 4);

    // Espera respuesta: 0x16 0x02 0x51 [MODE] [CS]
    memset(buf_msg, 0, CM1106_LEN_BUF_MSG);
    uint8_t nb = serial_read_bytes(5, CM1106_TIMEOUT);  // 5 bytes, timeout 5 segundos

    // Valida respuesta
    if (valid_response_len(CM1106_CMD_WORKING_STATUS, nb, 5)) {
        *mode = buf_msg[3];  // Extrae modo del byte 3
        result = true;
        CM1106_LOG("DEBUG: Successful getting working status\n");
    } else {
        CM1106_LOG("DEBUG: Error in getting working status!\n");
    }

    return result;
}
```

**Validación Interna:**
- Checksum: Verifica que `buf_msg[4] == calculate_cs(5)`
- Longitud: Verifica que `buf_msg[1] == 2` (5-3)
- ACK: Verifica que `buf_msg[0] == 0x16`
- Comando: Verifica que `buf_msg[2] == 0x51`

#### Función: `set_working_status(uint8_t mode)`

**Propósito:** Cambiar el modo de operación del sensor.

**Protocolo Exacto:**
```
Envío (5 bytes):
  [0x11] - Identificador de paquete
  [0x02] - Longitud (5-3=2)
  [0x51] - Comando
  [MODE] - 0x00 = Single, 0x01 = Continuous
  [CS]   - Checksum

Respuesta esperada (4 bytes):
  [0x16] - ACK
  [0x01] - Longitud (4-3=1)
  [0x51] - Comando eco
  [CS]   - Checksum
```

**Código de Librería:**
```c
bool CM1106_UART::set_working_status(uint8_t mode) {
    bool result = false;

    if ((mode == CM1106_SINGLE_MEASUREMENT || 
         mode == CM1106_CONTINUOUS_MEASUREMENT)) {

        // Prepara buffer con modo a establecer
        buf_msg[3] = mode;

        // Envía: 0x11 0x02 0x51 [MODE] [CS]
        send_cmd_data(CM1106_CMD_WORKING_STATUS, 5);

        // Espera respuesta: 0x16 0x01 0x51 [CS]
        memset(buf_msg, 0, CM1106_LEN_BUF_MSG);
        uint8_t nb = serial_read_bytes(4, CM1106_TIMEOUT);

        // Valida respuesta
        if (valid_response_len(CM1106_CMD_WORKING_STATUS, nb, 4)) {
            result = true;
            CM1106_LOG("DEBUG: Successful setting of measurement mode\n");
        } else {
            CM1106_LOG("DEBUG: Error in setting of measurement mode!\n");
        }

    } else {
        CM1106_LOG("DEBUG: Invalid measurement mode!\n");
    }

    return result;
}
```

### Implementación en Arduino (mi_cm1106.ino)

**Uso en setup():**
```c
void setupCM1106() {
    // Crear instancia de la librería
    sensor_CM1106 = new CM1106_UART(CM1106_serial);
    
    // 1. PASO CRÍTICO: Detectar modo actual
    uint8_t mode;
    if (sensor_CM1106->get_working_status(&mode)) {
        if (mode == CM1106_SINGLE_MEASUREMENT) {
            DEBUG_OUT.println("Detected: Single measurement mode");
            
            // 2. PASO CRÍTICO: Cambiar a continuo
            if (sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT)) {
                DEBUG_OUT.println("Successfully switched to continuous mode");
            }
        } else {
            DEBUG_OUT.println("Already in continuous measurement mode");
        }
    } else {
        DEBUG_OUT.println("ERROR: Could not detect working status");
    }
}
```

### Implementación ESPHome (cm1106sl_ns) - Estado Actual

**Estado:** ❌ **INCOMPLETO - FALTA DETECCIÓN Y CAMBIO DE MODO**

Archivo: [cm1106sl_ns.cpp](esphome/components/cm1106sl_ns/cm1106sl_ns.cpp)

**Lo que está implementado:**
```c
void CM1106SLNSComponent::setup() {
    // ✅ Envía comando de configuración de período (0x50)
    uint8_t cmd[7] = {0x11, 0x04, 0x50, period_h, period_l, smoothing, cs};
    // ✅ Valida respuesta de configuración
}
```

**Lo que FALTA (CRÍTICO):**
```c
void CM1106SLNSComponent::setup() {
    // ❌ NO IMPLEMENTADO: Detectar modo actual
    // uint8_t mode;
    // cm1106_get_working_status_(&mode);
    
    // ❌ NO IMPLEMENTADO: Cambiar a modo continuo
    // if (mode != CM1106_CONTINUOUS_MEASUREMENT) {
    //     cm1106_set_working_status_(CM1106_CONTINUOUS_MEASUREMENT);
    // }
    
    // Lo que sí hace (pero tarde):
    // Envía configuración de período
}
```

**Consecuencia del problema:**
```
Sensor arranca por defecto en modo Single (0x00)
        ↓
setup() envía configuración de período pero NO cambia modo
        ↓
Sensor sigue en modo Single y espera comandos (query)
        ↓
Componente espera frames continuos que nunca llegan
        ↓
Error permanente: "No data available in buffer"
```

### Comparativa: Secuencia de Setup

| Paso | Arduino (CORRECTO) | ESPHome (INCORRECTO) |
|------|---|---|
| 1 | `get_working_status()` → Detecta modo | ❌ Omitido |
| 2 | Si modo ≠ continuo → `set_working_status(0x01)` | ❌ Omitido |
| 3 | Espera confirmación del cambio | ❌ Omitido |
| 4 | Configura período y suavizado (0x50) | ✅ Presente |
| 5 | Inicia lectura de frames continuos | ❌ Falla porque sensor no está en continuo |

### Mapa de Funciones Necesarias para ESPHome

**De la librería `cm1106_uart.cpp/h`, se necesita implementar en ESPHome:**

```c
// Constantes (ya existen en cm1106sl_ns.h)
#define CM1106_CMD_WORKING_STATUS 0x51
#define CM1106_SINGLE_MEASUREMENT 0x00
#define CM1106_CONTINUOUS_MEASUREMENT 0x01

// Función 1: Detectar modo actual
bool cm1106_get_working_status_(uint8_t *mode) {
    // Envía: 0x11 0x01 0x51 [CS]
    // Recibe: 0x16 0x02 0x51 [MODE] [CS]
    // Retorna: true si OK, mode contiene 0x00 o 0x01
}

// Función 2: Cambiar modo
bool cm1106_set_working_status_(uint8_t mode) {
    // Envía: 0x11 0x02 0x51 [MODE] [CS]
    // Recibe: 0x16 0x01 0x51 [CS]
    // Retorna: true si OK
}
```

**Código base disponible en `cm1106_uart.cpp` líneas 368-420:**
- Estructura de mensaje (identificador, longitud, comando, checksum)
- Validación de respuesta (ACK vs NAK)
- Timeout de comunicación
- Logging de debug

---

## 13. Análisis Profundo de la Librería cm1106_uart

### Funciones Públicas Disponibles

| Función | Protocolo | Respuesta | Uso Crítico |
|---------|-----------|-----------|------------|
| `get_co2()` | 0x11 0x01 0x01 [CS] | 0x16 0x02 0x01 [CO2H] [CO2L] [CS] | Lectura de datos en modo query |
| `get_working_status(mode)` | 0x11 0x01 0x51 [CS] | 0x16 0x02 0x51 [MODE] [CS] | **CRÍTICO: Detectar modo** |
| `set_working_status(mode)` | 0x11 0x02 0x51 [MODE] [CS] | 0x16 0x01 0x51 [CS] | **CRÍTICO: Activar continuo** |
| `set_measurement_period()` | 0x11 0x04 0x50 [PH] [PL] [SM] [CS] | 0x16 0x01 0x50 [CS] | Configurar período (1-65535s) |
| `get_measurement_period()` | 0x11 0x01 0x50 [CS] | 0x16 0x04 0x50 [PH] [PL] [SM] [CS] | Leer configuración actual |
| `get_software_version()` | 0x11 0x01 0x1E [CS] | Variable (15 bytes típicamente) | Validar sensor es SL-NS |
| `get_serial_number()` | 0x11 0x01 0x1F [CS] | Variable (14 bytes) | Identificar unidad |
| `get_ABC()` | 0x11 0x01 0x0F [CS] | 0x16 0x06 0x0F [data] [CS] | Leer calibración automática |
| `set_ABC()` | 0x11 0x06 0x10 [data] [CS] | 0x16 0x01 0x10 [CS] | Configurar calibración automática |

### Funciones Privadas de Protocolo (Base para ESPHome)

```c
// Envío básico
void send_cmd(uint8_t cmd)
    → Envía: [0x11] [0x01] [cmd] [CS]

void send_cmd_data(uint8_t cmd, uint8_t size)
    → Ensambla mensaje completo con checksum automático

// Lectura con timeout
uint8_t serial_read_bytes(uint8_t max_bytes, int timeout_seconds)
    → Lee hasta max_bytes en timeout_seconds
    → Retorna número real de bytes leídos

// Validación
bool valid_response(uint8_t cmd, uint8_t nb)
    → Valida checksum y estructura
    
bool valid_response_len(uint8_t cmd, uint8_t nb, uint8_t len)
    → Valida longitud esperada también

// Checksum
uint8_t calculate_cs(uint8_t nb)
    → Complemento a dos: 256 - (sum % 256)
```

### Estructura del Mensaje

```
Envío (send):
  [0x11]      ← Identificador de paquete (constante)
  [length]    ← Longitud de datos = tamaño_total - 3
  [cmd]       ← Comando (0x01, 0x51, 0x50, etc.)
  [data...]   ← Parámetros opcionales
  [CS]        ← Checksum: 256 - ((0x11 + length + cmd + data) % 256)

Respuesta (receive):
  [0x16]      ← ACK (éxito) o [0x06] NAK (error)
  [length]    ← Longitud de datos
  [cmd_echo]  ← Eco del comando original
  [data...]   ← Datos de respuesta
  [CS]        ← Checksum validado por librería
```

### Validación de Checksum en la Librería

```c
uint8_t CM1106_UART::calculate_cs(uint8_t nb) {
    uint8_t cs = 0;
    
    // Suma bytes 0, 1, 2 y todos los datos
    cs = buf_msg[0] + buf_msg[1] + buf_msg[2];
    for (int i = 3; i < (nb-1); i++) {
        cs = cs + buf_msg[i];
    }
    
    // Complemento a dos
    cs = 256 - (cs % 256);
    
    return cs;
}

// Ejemplo:
// Mensaje: [0x11] [0x02] [0x51] [0x01] [CS]
// Suma: 0x11 + 0x02 + 0x51 + 0x01 = 0x65 (101 decimal)
// CS = 256 - (101 % 256) = 256 - 101 = 155 = 0x9B
```

### Timeout de Comunicación

```c
#define CM1106_TIMEOUT  5     // 5 segundos máximo para respuesta

// Implementación en serial_read_bytes:
time(&start_t);
while ((difftime(end_t, start_t) <= timeout_seconds) && !readed) {
    if(mySerial->available()) {
        nb = mySerial->readBytes(buf_msg, max_bytes);
        readed = true;
    }            
    time(&end_t);
}
```

### Resumen: Lo que ESPHome Necesita Implementar

**De la librería `cm1106_uart.cpp`, ESPHome necesita:**

```c
// 1. Envío de comando básico (sin datos)
void cm1106_send_cmd_(uint8_t cmd) {
    // [0x11] [0x01] [cmd] [CS]
}

// 2. Envío de comando con datos
void cm1106_send_cmd_data_(uint8_t cmd, uint8_t size, uint8_t *data) {
    // [0x11] [len-3] [cmd] [data...] [CS]
}

// 3. Lectura con timeout
uint8_t cm1106_serial_read_(uint8_t max_bytes, uint16_t timeout_ms) {
    // Retorna bytes leídos en buf_msg[]
}

// 4. Cálculo de checksum
uint8_t cm1106_calculate_cs_(uint8_t nb) {
    // 256 - (sum % 256)
}

// 5. Validación de respuesta
bool cm1106_valid_response_(uint8_t cmd, uint8_t nb) {
    // Verifica ACK (0x16), echo de cmd, checksum
}

// 6. Wrappers de alto nivel (lo que realmente se usa)
bool cm1106_get_working_status_(uint8_t *mode)
bool cm1106_set_working_status_(uint8_t mode)
bool cm1106_set_measurement_period_(uint16_t period, uint8_t smoothing)
```

---

## 13. Comparativa de Setup/Initialization

### Arduino (my_cm1106.ino) - CORRECTO

```c
void setupCM1106() {
    // Crear instancia de la librería
    sensor_CM1106 = new CM1106_UART(CM1106_serial);
    
    DEBUG_OUT.println("\n=== CM1106 Initialization ===");
    
    // 1. Obtener versión para confirmar comunicación
    sensor_CM1106->get_software_version(sensor.softver);
    DEBUG_OUT.printf("Version: %s\n", sensor.softver);
    
    // 2. ✅ PASO CRÍTICO 1: Detectar modo actual
    uint8_t mode;
    if (sensor_CM1106->get_working_status(&mode)) {
        if (mode == CM1106_SINGLE_MEASUREMENT) {
            DEBUG_OUT.println("Mode: Single");
            
            // 3. ✅ PASO CRÍTICO 2: Cambiar a continuo
            DEBUG_OUT.println("Switching to Continuous...");
            if (sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT)) {
                DEBUG_OUT.println("Mode: Continuous (activated)");
            } else {
                DEBUG_OUT.println("ERROR: Failed to set continuous mode!");
            }
        } else {
            DEBUG_OUT.println("Mode: Continuous (already active)");
        }
    } else {
        DEBUG_OUT.println("ERROR: Could not read working status!");
    }
    
    // 4. Obtener y mostrar período actual
    int16_t period;
    uint8_t smoothed;
    if (sensor_CM1106->get_measurement_period(&period, &smoothed)) {
        DEBUG_OUT.printf("Period: %d seconds\n", period);
        DEBUG_OUT.printf("Smoothed: %d samples\n", smoothed);
    }
    
    // 5. Loop de lectura (sensor ya está en continuo)
}
```

**Resultado esperado en Arduino:**
```
=== CM1106 Initialization ===
Version: V06.02
Mode: Single
Switching to Continuous...
Mode: Continuous (activated)
Period: 2 seconds
Smoothed: 1 samples
```

### ESPHome (cm1106sl_ns.cpp) - ACTUAL (INCORRECTO)

```c
void CM1106SLNSComponent::setup() {
    // ❌ NO DETECTA MODO ACTUAL
    // ❌ NO CAMBIA A CONTINUO
    // Directamente intenta enviar configuración:
    
    uint8_t cmd[7] = {
        0x11,           // Identificador
        0x04,           // Longitud
        0x50,           // Comando (measurement period)
        period_h,       // Período alto
        period_l,       // Período bajo
        smoothing,      // Suavizado
        checksum        // Checksum
    };
    
    this->write_array(cmd, 7);
    
    // Espera respuesta de configuración
    // PERO: Sensor está en modo Single, así que ignora este comando
    //       y el setup se congela o falla
}
```

### ESPHome (cm1106sl_ns.cpp) - CORRECTO (PROPUESTO)

```c
void CM1106SLNSComponent::setup() {
    ESP_LOGCONFIG(TAG, "Initializing CM1106SL-NS sensor");
    
    // 1. ✅ PASO CRÍTICO 1: Detectar modo actual
    ESP_LOGCONFIG(TAG, "Detecting current working mode...");
    uint8_t current_mode = 0xFF;
    
    if (!this->cm1106_get_working_status_(&current_mode)) {
        ESP_LOGE(TAG, "Failed to detect working status!");
        this->mark_failed();
        return;
    }
    
    if (current_mode == CM1106_SINGLE_MEASUREMENT) {
        ESP_LOGCONFIG(TAG, "Current mode: Single Measurement");
        
        // 2. ✅ PASO CRÍTICO 2: Cambiar a continuo
        ESP_LOGCONFIG(TAG, "Switching to Continuous mode...");
        
        if (!this->cm1106_set_working_status_(CM1106_CONTINUOUS_MEASUREMENT)) {
            ESP_LOGE(TAG, "Failed to switch to continuous mode!");
            this->mark_failed();
            return;
        }
        
        ESP_LOGCONFIG(TAG, "Successfully switched to Continuous mode");
        
        // Esperar un poco después del cambio de modo
        delay(500);
        
    } else if (current_mode == CM1106_CONTINUOUS_MEASUREMENT) {
        ESP_LOGCONFIG(TAG, "Already in Continuous mode");
    } else {
        ESP_LOGE(TAG, "Unknown mode: 0x%02X", current_mode);
        this->mark_failed();
        return;
    }
    
    // 3. Configurar período y suavizado
    ESP_LOGCONFIG(TAG, "Configuring measurement period...");
    
    uint16_t period = this->measurement_period_;  // e.g., 2 segundos
    uint8_t smoothing = this->smoothing_samples_;  // e.g., 1
    
    if (!this->cm1106_set_measurement_period_(period, smoothing)) {
        ESP_LOGE(TAG, "Failed to set measurement period!");
        this->mark_failed();
        return;
    }
    
    ESP_LOGCONFIG(TAG, "Configuration complete. Sensor ready for continuous reading.");
    
    // Inicializar timestamp de última lectura
    this->last_read_ms_ = millis();
}

// Función privada: Leer modo actual
bool CM1106SLNSComponent::cm1106_get_working_status_(uint8_t *mode) {
    // Preparar comando: 0x11 0x01 0x51 [CS]
    uint8_t cmd[4];
    cmd[0] = 0x11;  // Identificador
    cmd[1] = 0x01;  // Longitud
    cmd[2] = 0x51;  // Comando (working status)
    cmd[3] = this->calculate_cs_(cmd, 3);  // Checksum
    
    // Enviar comando
    this->write_array(cmd, 4);
    
    // Esperar respuesta: 0x16 0x02 0x51 [MODE] [CS] (5 bytes)
    uint8_t response[5];
    if (!this->read_array(response, 5, 5000)) {  // 5 segundo timeout
        ESP_LOGE(TAG, "Timeout reading working status response");
        return false;
    }
    
    // Validar respuesta
    if (response[0] != 0x16 || response[2] != 0x51) {
        ESP_LOGE(TAG, "Invalid response for working status");
        return false;
    }
    
    // Validar checksum
    if (response[4] != this->calculate_cs_(response, 4)) {
        ESP_LOGE(TAG, "Checksum mismatch in working status response");
        return false;
    }
    
    // Extraer modo
    *mode = response[3];
    return true;
}

// Función privada: Cambiar modo
bool CM1106SLNSComponent::cm1106_set_working_status_(uint8_t mode) {
    // Preparar comando: 0x11 0x02 0x51 [MODE] [CS]
    uint8_t cmd[5];
    cmd[0] = 0x11;  // Identificador
    cmd[1] = 0x02;  // Longitud
    cmd[2] = 0x51;  // Comando
    cmd[3] = mode;  // Modo (0x00=Single, 0x01=Continuous)
    cmd[4] = this->calculate_cs_(cmd, 4);  // Checksum
    
    // Enviar comando
    this->write_array(cmd, 5);
    
    // Esperar respuesta: 0x16 0x01 0x51 [CS] (4 bytes)
    uint8_t response[4];
    if (!this->read_array(response, 4, 5000)) {  // 5 segundo timeout
        ESP_LOGE(TAG, "Timeout reading working status change response");
        return false;
    }
    
    // Validar respuesta
    if (response[0] != 0x16 || response[2] != 0x51) {
        ESP_LOGE(TAG, "Invalid response for working status change");
        return false;
    }
    
    // Validar checksum
    if (response[3] != this->calculate_cs_(response, 3)) {
        ESP_LOGE(TAG, "Checksum mismatch in working status change response");
        return false;
    }
    
    return true;
}

// Función privada: Configurar período
bool CM1106SLNSComponent::cm1106_set_measurement_period_(uint16_t period, uint8_t smoothing) {
    // Preparar comando: 0x11 0x04 0x50 [PH] [PL] [SM] [CS]
    uint8_t cmd[7];
    cmd[0] = 0x11;  // Identificador
    cmd[1] = 0x04;  // Longitud
    cmd[2] = 0x50;  // Comando (measurement period)
    cmd[3] = (period >> 8) & 0xFF;  // Período alto
    cmd[4] = period & 0xFF;         // Período bajo
    cmd[5] = smoothing;             // Suavizado
    cmd[6] = this->calculate_cs_(cmd, 6);  // Checksum
    
    // Enviar comando
    this->write_array(cmd, 7);
    
    // Esperar respuesta: 0x16 0x01 0x50 [CS] (4 bytes)
    uint8_t response[4];
    if (!this->read_array(response, 4, 5000)) {
        ESP_LOGE(TAG, "Timeout reading measurement period response");
        return false;
    }
    
    // Validar respuesta
    if (response[0] != 0x16 || response[2] != 0x50) {
        ESP_LOGE(TAG, "Invalid response for measurement period");
        return false;
    }
    
    // Validar checksum
    if (response[3] != this->calculate_cs_(response, 3)) {
        ESP_LOGE(TAG, "Checksum mismatch in measurement period response");
        return false;
    }
    
    return true;
}
```

---

## 14. Comparativa de Setup/Initialization Resumida
```c
void setupCM1106() {
    // 1. Crear instancia de librería
    sensor_CM1106 = new CM1106_UART(CM1106_serial);
    
    // 2. Detectar versión (para confirmar comunicación)
    sensor_CM1106->get_software_version(sensor.softver);
    
    // 3. **DETECTAR MODO ACTUAL** ← ESPHome NO hace esto
    uint8_t mode;
    if (sensor_CM1106->get_working_status(&mode)) {
        if (mode == CM1106_SINGLE_MEASUREMENT) {
            DEBUG_OUT.println("Single");
            // 4. **CAMBIAR A CONTINUO** ← ESPHome NO hace esto
            sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT);
        } else {
            DEBUG_OUT.println("Continuous");
        }
    }
    
    // 5. Obtener período y suavizado actuales
    int16_t period;
    uint8_t smoothed;
    sensor_CM1106->get_measurement_period(&period, &smoothed);
    
    // 6. En setup() loop: leer CO2 con get_co2() en modo query
}
```

### ESPHome (cm1106sl_ns.cpp)
```c
void CM1106SLNSComponent::setup() {
    // 1. ✅ Enviar comando de configuración
    uint8_t cmd[7] = {0x11, 0x04, 0x50, df1, df2, df3, cs};
    
    // 2. ✅ Validar respuesta
    if (response[0] == 0x16 && response[1] == 0x01) {
        ESP_LOGCONFIG("Configuration successful");
    }
    
    // 3. ❌ FALTA: Detectar modo actual (get_working_status)
    // 4. ❌ FALTA: Cambiar a continuo si es necesario (set_working_status)
}
```

---

## Conclusiones y Recomendaciones

### 🎯 Hallazgos Clave de la Librería `cm1106_uart`

1. **La librería Arduino tiene TODO lo necesario** - Incluye todas las funciones para:
   - ✅ Detectar modo actual (`get_working_status`)
   - ✅ Cambiar a continuo (`set_working_status`)
   - ✅ Configurar período (`set_measurement_period`)
   - ✅ Leer datos en continuo (`get_co2` automático)

2. **Protocolo está bien documentado en la librería** - El código `.cpp` muestra exactamente:
   - Bytes a enviar en cada comando
   - Bytes esperados en cada respuesta
   - Validación de checksum y ACK
   - Timeouts (5 segundos)

3. **La librería abstrae correctamente UART** - Encapsula:
   - Cálculo de checksum (complemento a dos)
   - Validación de respuestas
   - Manejo de timeouts
   - Logging de debug

### 🔴 Problema en ESPHome: Secuencia de Setup Incompleta

**El componente `cm1106sl_ns` omite dos pasos CRÍTICOS:**

| Paso | Función Requerida | Arduino | ESPHome |
|------|---|---|---|
| 1 | Detectar modo | `get_working_status()` | ❌ OMITIDO |
| 2 | Activar continuo | `set_working_status(0x01)` | ❌ OMITIDO |
| 3 | Configurar período | `set_measurement_period()` | ✅ Presente |
| 4 | Leer frames | `get_co2()` en loop | ❌ Espera infinitamente |

**Consecuencia:** Sin los pasos 1 y 2, el sensor nunca entra en modo continuo.

### ✅ Solución Completa

**Implementar en `cm1106sl_ns.cpp`:**

```c
// Header: cm1106sl_ns.h
private:
  bool cm1106_get_working_status_(uint8_t *mode);
  bool cm1106_set_working_status_(uint8_t mode);
  bool cm1106_set_measurement_period_(uint16_t period, uint8_t smoothing);
  uint8_t calculate_cs_(uint8_t *buffer, uint8_t size);

// Implementation: cm1106sl_ns.cpp
void CM1106SLNSComponent::setup() {
  // 1. Detectar modo
  uint8_t mode;
  if (!cm1106_get_working_status_(&mode)) {
    mark_failed();
    return;
  }
  
  // 2. Si no es continuo, cambiar
  if (mode != 0x01) {
    if (!cm1106_set_working_status_(0x01)) {
      mark_failed();
      return;
    }
  }
  
  // 3. Configurar período
  if (!cm1106_set_measurement_period_(measurement_period_, smoothing_)) {
    mark_failed();
    return;
  }
  
  // Setup completado exitosamente
}
```

**Tiempo estimado de implementación:** 1-2 horas
- Copiar estructura de mensajes de `cm1106_uart.cpp`
- Implementar 3 funciones privadas
- Integrar en `setup()`
- Agregar logging

### 📊 Comparativa: Documentación vs Librería vs ESPHome

| Aspecto | UART Doc | Arduino Lib | ESPHome Actual | ESPHome Requerido |
|--------|---|---|---|---|
| Baudrate 9600 | ✅ | ✅ | ✅ | ✅ |
| Checksum | ✅ | ✅ | ✅ | ✅ |
| Cmd: Período | ✅ | ✅ | ✅ | ✅ |
| Cmd: Modo GET | ❓ Implícito | ✅ | ❌ | ⚠️ Requerido |
| Cmd: Modo SET | ❓ Implícito | ✅ | ❌ | ⚠️ Requerido |
| Timeout | Implícito | ✅ | ? | ⚠️ Recomendado |
| Validación ACK | Implícita | ✅ | ✅ | ✅ |
| Continuous Frames | ✅ | ✅ Automático | ❌ | ⚠️ Bloqueado por modo |

### 🎓 Lecciones Aprendidas

1. **Documentación UART es incompleta** - No menciona explícitamente comandos de modo (0x51)
   - Se pueden inferir del protocolo general
   - Arduino los implementa correctamente

2. **La librería Arduino es la referencia** - Usar como:
   - Base para porting a ESPHome
   - Fuente de protocolo exacto
   - Validación de implementación

3. **El modo es crítico** - Sin detectar y cambiar el modo:
   - Sensor usa modo single (por defecto)
   - No envía frames continuos
   - Componente espera datos que nunca llegan

### 📋 Checklist de Implementación

- [ ] **Paso 1:** Agregar constantes de modo a `cm1106sl_ns.h`
  ```c
  #define CM1106_CMD_WORKING_STATUS 0x51
  #define CM1106_SINGLE_MEASUREMENT 0x00
  #define CM1106_CONTINUOUS_MEASUREMENT 0x01
  ```

- [ ] **Paso 2:** Implementar `calculate_cs_()` (copiar de cm1106_uart.cpp)

- [ ] **Paso 3:** Implementar `cm1106_get_working_status_()` 
  - Enviar: 0x11 0x01 0x51 [CS]
  - Recibir: 0x16 0x02 0x51 [MODE] [CS]

- [ ] **Paso 4:** Implementar `cm1106_set_working_status_()`
  - Enviar: 0x11 0x02 0x51 [MODE] [CS]
  - Recibir: 0x16 0x01 0x51 [CS]

- [ ] **Paso 5:** Implementar `cm1106_set_measurement_period_()`
  - Enviar: 0x11 0x04 0x50 [PH] [PL] [SM] [CS]
  - Recibir: 0x16 0x01 0x50 [CS]

- [ ] **Paso 6:** Integrar en `setup()` con orden correcto:
  1. Detectar modo
  2. Cambiar si necesario
  3. Configurar período
  4. Iniciar lectura de frames

- [ ] **Paso 7:** Agregar logging de debug para cada paso

- [ ] **Paso 8:** Probar con sensor físico

### 🔗 Referencias en Código

- **Arduino Library:** `/arduino/cm1106_uart.cpp` (líneas 260-420)
- **Protocolo exacto:** Ver comentarios en funciones `get_working_status()` y `set_working_status()`
- **ESPHome Base:** `/esphome/components/cm1106sl_ns/cm1106sl_ns.cpp`

---

## Resumen Ejecutivo para Desarrollador

**El problema:** ESPHome no cambia el sensor a modo continuo.

**La causa:** `setup()` omite dos llamadas críticas.

**La solución:** Portar `get_working_status()` y `set_working_status()` de la librería Arduino.

**Dificultad:** Baja - Código bien documentado y disponible.

**Beneficio:** Sensores CM1106SL-NS funcionando 100% en ESPHome con comunicación continua automática.
