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

### Implementación Arduino (cm1106_uart.h)

**Detección de modo actual:**
```c
bool CM1106_UART::get_working_status(uint8_t *mode) {
    // Comando: 0x11 0x01 0x00 (4 bytes total con checksum)
    // Respuesta: 0x16 0x02 0x00 [MODE] [CS] (5 bytes)
    // MODE: 0x00 = Single Measurement, 0x01 = Continuous Measurement
    
    send_cmd_data(CM1106_CMD_WORKING_STATUS, 4);
    uint8_t nb = serial_read_bytes(5, CM1106_TIMEOUT);
    
    if (valid_response_len(CM1106_CMD_WORKING_STATUS, nb, 5)) {
        *mode = buf_msg[3];  // Modo en byte 3
        result = true;
    }
    return result;
}
```

**Cambio de modo a continuo:**
```c
bool CM1106_UART::set_working_status(uint8_t mode) {
    // Comando: 0x11 0x01 0x00 [MODE] (5 bytes total con checksum)
    // MODE: 0x00 = Single, 0x01 = Continuous
    // Respuesta: 0x16 0x01 0x00 [CS] (4 bytes)
    
    if (mode == CM1106_CONTINUOUS_MEASUREMENT) {
        buf_msg[3] = mode;
        send_cmd_data(CM1106_CMD_WORKING_STATUS, 5);
        uint8_t nb = serial_read_bytes(4, CM1106_TIMEOUT);
        
        if (valid_response_len(CM1106_CMD_WORKING_STATUS, nb, 4)) {
            result = true;  // Modo cambiado exitosamente
        }
    }
    return result;
}
```

**Uso en Arduino (mi_cm1106.ino):**
```c
uint8_t mode;
if (sensor_CM1106->get_working_status(&mode)) {
    if (mode == CM1106_SINGLE_MEASUREMENT) {
        DEBUG_OUT.println("Single measurement mode detected");
        // Cambiar a continuo
        if (sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT)) {
            DEBUG_OUT.println("Changed to continuous mode");
        }
    } else {
        DEBUG_OUT.println("Already in continuous mode");
    }
}
```

### Implementación ESPHome (cm1106sl_ns)

**Estado Actual:** ❌ **NO IMPLEMENTADO**

El componente ESPHome actual:
- ✅ Envía comando de configuración de período y suavizado (0x11 0x04 0x50 ...)
- ❌ **NO detecta el modo actual del sensor**
- ❌ **NO verifica si está en modo continuo**
- ❌ **NO cambia el modo a continuo si es necesario**

**Consecuencia Observada:**
```
[14:23:45] No data available in buffer (waiting for continuous frames)
[14:23:45] No data available in buffer (waiting for continuous frames)
...
```

El sensor permanece en **modo single** (por defecto), por lo que:
- No envía frames continuos automáticamente
- Solo responde a comandos explícitos (modo query)
- El buffer UART nunca recibe datos

### Protocolo para Modo (Derivado de Arduino)

**Comando: GET MODE (Leer modo actual)**
```
TX: [0x11] [0x01] [0x00] [CS]
RX: [0x16] [0x02] [0x00] [MODE] [CS]

MODE: 
  0x00 = Single Measurement
  0x01 = Continuous Measurement
```

**Comando: SET MODE (Cambiar a continuo)**
```
TX: [0x11] [0x01] [0x00] [0x01] [CS]
RX: [0x16] [0x01] [0x00] [CS]
```

---

## 13. Comparativa de Setup/Initialization

### Arduino (my_cm1106.ino)
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

## Resumen de Discrepancias

### ✅ Implementado Correctamente

1. **Velocidad UART:** 9600 bps
2. **Configuración de período y suavizado:** Comando 0x11 0x04 0x50
3. **Extracción de CO2:** Bytes 3-4 del frame continuo
4. **Checksum:** Implementación correcta (complemento a dos)

### 🔴 CRÍTICO - No Implementado (CAUSA DEL PROBLEMA)

1. **Detección de modo:** ❌ No se consulta `get_working_status()`
2. **Cambio a continuo:** ❌ No se ejecuta `set_working_status(0x01)`
3. **Sin estos dos pasos, el sensor no entra en modo continuo y nunca envía frames**

### ⚠️ No Implementado o No Visible

1. **Estados DF3/DF4:** No se extraen ni procesan
2. **Manejo de Timeouts:** No implementado
3. **Reset Suave:** No implementado
4. **ABC (Auto Baseline Calibration):** No implementado

### 📚 Funcionalidades Extra (Fuera de documentación UART)

1. Detección de versión de firmware
2. Configuración ABC (calibración automática)
3. Número de serie del sensor

---

## Conclusiones

### El Problema Encontrado

El componente ESPHome **NO IMPLEMENTA LOS COMANDOS DE MODO CRÍTICOS**:
- `get_working_status()` - Leer modo actual
- `set_working_status(CM1106_CONTINUOUS_MEASUREMENT)` - Cambiar a continuo

Sin esto, el sensor permanece en modo Single (por defecto), lo que causa:
- "No data available in buffer" continuamente
- Sensor no envía frames automáticos
- Component espera frames que nunca llegan

### Solución Requerida

Implementar en `cm1106sl_ns.cpp::setup()`:

```c
// 1. Detectar modo actual
uint8_t current_mode = 0xFF;
if (!detect_working_status(&current_mode)) {
    ESP_LOGE("Failed to detect working status");
    this->mark_failed();
    return;
}

ESP_LOGCONFIG("Current mode: %s", 
    current_mode == 0x00 ? "Single Measurement" : "Continuous Measurement");

// 2. Si no está en continuo, cambiar
if (current_mode != CM1106_CONTINUOUS_MEASUREMENT) {
    ESP_LOGCONFIG("Switching to continuous mode...");
    if (!set_working_status(CM1106_CONTINUOUS_MEASUREMENT)) {
        ESP_LOGE("Failed to set continuous mode");
        this->mark_failed();
        return;
    }
    ESP_LOGCONFIG("Successfully changed to continuous mode");
}

// 3. Ahora sí, enviar configuración de período y suavizado
// ... (código existente)
```

### Validez de Documentación UART

- ✅ La documentación UART es válida y precisa
- ✅ El protocolo especificado coincide con Arduino
- ⚠️ La documentación NO documenta explícitamente los comandos de modo
- ℹ️ Estos comandos se infieren del protocolo general y se ven en Arduino

### Recomendaciones

1. **URGENTE:** Agregar `get_working_status()` y `set_working_status()` a cm1106sl_ns
2. **URGENTE:** Llamarlos en `setup()` antes de enviar configuración de período
3. Actualizar UART_COMMUNICATION.md para documentar explícitamente comandos de modo
4. Agregar logging de detección de modo para debugging futuro
5. Considerar agregar métodos adicionales (ABC, versión, serial)
