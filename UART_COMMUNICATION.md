# Comunicación UART - Sensor CM1106SL_NS

## Configuración Física UART

| Parámetro | Valor |
|-----------|-------|
| **Velocidad (Baud Rate)** | 9600 bps |
| **Bits de Datos** | 8 bits |
| **Paridad** | Ninguna |
| **Bits de Parada** | 1 |
| **Control de Flujo** | Ninguno |

---

## Estructura de Frames

### Frame de Datos (Lectura Continua)

**Tamaño:** 8 bytes

```
[Byte0] [Byte1] [Byte2] [Byte3] [Byte4] [Byte5] [Byte6] [Byte7]
 0x16    0x05   0x50   [CO2H]  [CO2L]  [DF3]   [DF4]   [CS]
```

| Campo | Bytes | Descripción |
|-------|-------|-------------|
| **Encabezado** | 0-1 | `0x16 0x05` - Identifica frame de datos |
| **Comando** | 2 | `0x50` - Lectura de datos |
| **CO2 Alto** | 3 | MSB de concentración CO2 (ppm) |
| **CO2 Bajo** | 4 | LSB de concentración CO2 (ppm) |
| **DF3** | 5 | Byte de estado del sensor (ver tabla de estados) |
| **DF4** | 6 | Byte adicional de información |
| **Checksum** | 7 | Validación de integridad (Two's complement) |

### Extracción de Datos

```c
uint16_t co2_ppm = (buffer[3] << 8) | buffer[4];  // CO2 en ppm
uint8_t status = buffer[5];                        // DF3: Estado del sensor
uint8_t info = buffer[6];                          // DF4: Información adicional
```

---

## Cálculo de Checksum

El checksum es el **complemento a dos** de la suma de los 7 primeros bytes.

### Fórmula de Cálculo

```c
uint8_t calculate_checksum(const uint8_t *buffer, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += buffer[i];
    }
    return (~sum) + 1;  // Complemento a uno + 1
}
```

### Validación

```c
bool validate_checksum(const uint8_t *buffer, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len - 1; i++) {
        sum += buffer[i];
    }
    uint8_t expected = (~sum) + 1;
    return expected == buffer[len - 1];
}
```

### Ejemplo

```
Frame: 16 05 50 00 C8 00 00 ??
Suma (7 bytes):  0x16 + 0x05 + 0x50 + 0x00 + 0xC8 + 0x00 + 0x00 = 0x133
Complemento a uno: ~0x33 = 0xCC
Complemento a dos: 0xCC + 1 = 0xCD
Checksum = 0xCD
```

---

## Estados del Sensor (DF3)

| DF3 | Descripción | Acción |
|-----|-------------|--------|
| `0x08` | **Calentamiento en curso** | Esperar hasta que complete (timeout configurable) |
| `0x00` | **Normal** | Funcionamiento correcto |
| `0x01` | **Error del sensor** | Revisar conexión física |
| `0x02` | **Calibración requerida** | Realizar calibración del sensor |

---

## Modo Continuo (Continuous Mode)

El sensor opera por defecto en **modo continuo**, enviando datos automáticamente a intervalos regulares.

### Configuración del Modo Continuo

Se configura mediante el comando de configuración que establece el período de medición.

#### Comando de Configuración

**Tamaño:** 7 bytes

```
[Byte0] [Byte1] [Byte2] [Byte3] [Byte4] [Byte5] [Byte6]
 0x11    0x04   0x50   [DF1]   [DF2]   [DF3]   [CS]
```

| Campo | Bytes | Descripción |
|-------|-------|-------------|
| **Encabezado** | 0-1 | `0x11 0x04` - Comando de configuración |
| **Comando** | 2 | `0x50` - Configurar medición |
| **Período Alto** | 3 | MSB del período (DF1 = Period_s / 256) |
| **Período Bajo** | 4 | LSB del período (DF2 = Period_s % 256) |
| **Muestras Suavizado** | 5 | DF3 = Número de muestras para promediado |
| **Checksum** | 6 | Validación del comando |

#### Respuesta del Sensor

El sensor responde con 4 bytes de confirmación:

```
[Byte0] [Byte1] [Byte2] [Byte3]
 0x16    0x01   0x50   [CS]
```

- `0x16 0x01` - Confirmación
- `0x50` - Echo del comando
- `CS` - Checksum de validación

### Parámetros de Configuración

| Parámetro | Rango | Descripción |
|-----------|-------|-------------|
| **Período** | 1-65535 segundos | Intervalo entre mediciones consecutivas |
| **Muestras Suavizado** | 1-255 | Número de muestras para promediado |

### Ejemplo de Configuración en C

```c
// Configurar período de 10 segundos con 32 muestras de suavizado
uint16_t period_s = 10;
uint8_t smoothing_samples = 32;

uint8_t df1 = period_s / 256;           // 0x00
uint8_t df2 = period_s % 256;           // 0x0A
uint8_t df3 = smoothing_samples;        // 0x20

uint8_t cmd[7] = {0x11, 0x04, 0x50, df1, df2, df3, 0x00};
cmd[6] = calculate_checksum(cmd, 6);

// Enviar comando por UART
uart_write_bytes(cmd, 7);

// Esperar respuesta (timeout máximo 2 segundos)
// Leer 4 bytes: 0x16 0x01 0x50 [CS]
```

### Timing del Modo Continuo

```
Envío Config    Espera Respuesta    Lectura Continua
    |                   |                    |
    v                   v                    v
[0x11...]--->(max 2s)--[0x16 0x01 0x50...]-->[frames cada Period_s segundos]
```

**Tiempos Típicos:**
- Respuesta a comando: < 100 ms
- Timeout de respuesta: 2000 ms
- Primera medición: Inmediata después de confirmación

---

## Modo Single (Disparo Único)

En modo single/single-shot, el sensor se configura para enviar una única medición bajo demanda.

### Implementación del Modo Single

Se logra configurando un período de medición **muy largo** o mediante protocolo específico:

#### Opción 1: Período Largo

```c
// Configurar con período de 65535 segundos (máximo)
// El sensor enviará una medición, luego esperará 65535s para la siguiente
uint8_t cmd[7] = {0x11, 0x04, 0x50, 0xFF, 0xFF, 0x01, 0x00};
cmd[6] = calculate_checksum(cmd, 6);
uart_write_bytes(cmd, 7);
```

#### Opción 2: Comando de Lectura Manual (si está soportado)

```c
// Comando para lectura única
// Nota: Verificar datasheet del CM1106SL_NS para comando específico
uint8_t single_shot_cmd[4] = {0x11, 0x01, 0x50, 0x00};
single_shot_cmd[3] = calculate_checksum(single_shot_cmd, 3);
```

### Diagrama de Tiempo - Modo Single

```
Inicial          Solicitud         Respuesta        Siguiente Solicitud
   |                 |                 |                    |
   v                 v                 v                    v
[Config]-->(OK)--[esperar]-->(timeout 65535s)-->[siguiente ciclo]
                    |                 |
                    +--timeout 2s-----+
                    
Si no hay datos en 2s: timeout error
```

---

## Validación de Rango CO2

El sensor valida que las lecturas estén en el rango operativo:

```c
#define CO2_MIN_VALID 300   // ppm
#define CO2_MAX_VALID 5000  // ppm

uint16_t co2 = (buffer[3] << 8) | buffer[4];

if (co2 == 0 || co2 < CO2_MIN_VALID || co2 > CO2_MAX_VALID) {
    // Lectura inválida, ignorar
    continue;
}
```

---

## Manejo de Errores

### Timeout de Datos

Si no se reciben datos dentro del período configurado + margen:

```c
#define TIMEOUT_MARGIN 500  // ms
uint32_t timeout = measurement_period + TIMEOUT_MARGIN;

if (millis() - last_frame_time > timeout) {
    // Timeout: No hay datos del sensor
    // Acciones:
    // - Marcar sensor como no disponible
    // - Logs de error
    // - Intentar reset suave si fallos repetidos
}
```

### Detección de Frames Corruptos

```c
uint8_t bad_frames_counter = 0;
const uint8_t MAX_BAD_FRAMES = 5;

if (!validate_checksum(buffer, 8)) {
    bad_frames_counter++;
    if (bad_frames_counter > MAX_BAD_FRAMES) {
        soft_reset();        // Hacer reset suave del sensor
        bad_frames_counter = 0;
    }
}
```

### Reset Suave del Sensor

```c
void soft_reset() {
    uint8_t reset_cmd[5] = {0x11, 0x03, 0x02, 0x00, 0xED};
    uart_write_bytes(reset_cmd, 5);
    // Esperar ~1 segundo antes de enviar comandos
    delay(1000);
}
```

---

## Secuencia de Inicialización

```
1. Configurar UART a 9600 bps
                     |
2. Enviar Comando de Configuración (0x11 0x04 0x50 ...)
                     |
3. Esperar Respuesta (0x16 0x01 0x50 ...)
                     |
             ¿Timeout?
               /      \
             Sí        No
             |          |
        Reset    4. Esperar datos continuos
        suave       |
                    v
            Leer frames cada Period_s
                    |
                    v
            Procesar CO2, DF3, DF4
```

---

## Comparativa: Modo Continuo vs Modo Single

| Característica | Modo Continuo | Modo Single |
|---|---|---|
| **Frecuencia** | Periódica automática | Bajo demanda |
| **Período** | Configurable (1-65535s) | Única lectura |
| **Consumo** | Continuo | Intermitente (bajo) |
| **Latencia** | Período configurado | Inmediata |
| **Uso Típico** | Monitoreo constante | Muestreo ocasional |
| **Implementación** | Directa con config | Período largo |

---

## Referencia Rápida de Comandos

### Lectura Continua (Respuesta del Sensor)

```
Entrada automática cada Period_s segundos

[16] [05] [50] [CO2H] [CO2L] [DF3] [DF4] [CS]
```

### Configuración del Período

```
Envío: [11] [04] [50] [DF1] [DF2] [DF3] [CS]
       donde DF1 = period/256, DF2 = period%256

Respuesta: [16] [01] [50] [CS]
```

### Reset Suave

```
Envío: [11] [03] [02] [00] [ED]
```

---

## Notas Importantes

1. **Velocidad UART:** Siempre 9600 bps, no configurable
2. **Timeout de Respuesta:** 2 segundos máximo para confirmación
3. **Período Mínimo:** 1 segundo recomendado
4. **Calentamiento:** El sensor requiere 15-60 segundos tras encendido
5. **Rango CO2:** 0-5000 ppm (valores fuera se descartan)
6. **Checksum Obligatorio:** Todos los comandos requieren checksum válido
7. **Buffer UART:** Se recomienda buffer de al menos 64 bytes para evitar pérdidas

---

## Referencias

- **Datasheet:** CM1106SL_NS
- **Protocolo:** UART Half-Duplex
- **Estado Sensor:** DF3 = bits de estado, DF4 = información adicional
- **Estabilidad:** Se calcula con contador de estabilidad basado en variación < 20 ppm
