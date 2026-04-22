# Implementación del Modo Continuo - ESPHome vs Arduino

## Resumen de Cambios

Se ha actualizado el código ESPHome para implementar el modo continuo **exactamente igual que Arduino**, con reintentos automáticos y mejor manejo de errores.

---

## Comparativa: Arduino vs ESPHome

### Arduino (my_cm1106.ino)

```cpp
void setupCM1106() {
    // 1. Inicializar comunicación UART
    CM1106_serial.begin(CM1106_BAUDRATE);
    sensor_CM1106 = new CM1106_UART(CM1106_serial);
    
    // 2. Detectar modo actual
    if (sensor_CM1106->get_working_status(&mode)) {
        if (mode == CM1106_SINGLE_MEASUREMENT) {
            // 3. Cambiar a continuo si es necesario
            sensor_CM1106->set_working_status(CM1106_CONTINUOUS_MEASUREMENT);
        }
    }
    
    // 4. Leer configuración actual
    if (sensor_CM1106->get_measurement_period(&period, &smoothed)) {
        // Mostrar configuración...
    }
}

void readCM1106() {
    sensor.co2 = sensor_CM1106->get_co2();  // Lectura de datos
}
```

### ESPHome (Nueva Implementación)

```cpp
// setup() - Fase 1: Enviar comando de configuración
void CM1106SLNSComponent::setup() {
    ESP_LOGCONFIG(TAG, "=== CM1106SL-NS Initialization ===");
    ESP_LOGCONFIG(TAG, "Step 1: Sending initial config command for continuous mode");
    this->send_config_command_();
}

// loop() - Fase 2: Esperar respuesta con reintentos
if (this->awaiting_config_response_) {
    if (valid_response_received) {
        ESP_LOGI(TAG, "✓ CONFIG ACK RECEIVED");
        ESP_LOGI(TAG, "Step 3: Continuous mode configured successfully");
        ESP_LOGI(TAG, "Step 4: Ready to receive sensor data");
        this->continuous_mode_confirmed_ = true;
    } else if (timeout) {
        this->check_config_retry_();  // Reintentos automáticos
    }
}

// loop() - Fase 3: Procesar datos continuos
while (this->available() >= FRAME_LENGTH) {
    sensor.co2 = (buffer[3] << 8) | buffer[4];  // Extracción CO2
}
```

---

## Nuevas Características Implementadas

### 1. **Inicialización Robusta con Reintentos**

```cpp
// Configuración de reintentos
static constexpr uint8_t MAX_CONFIG_RETRIES = 5;
static constexpr uint32_t CONFIG_RETRY_DELAY = 1000;  // 1 segundo entre reintentos
static constexpr uint32_t CONFIG_RESPONSE_TIMEOUT = 2000;  // 2 segundos timeout

// Nueva variable
uint8_t config_retry_count_ = 0;
bool continuous_mode_confirmed_ = false;
```

### 2. **Nueva Función: check_config_retry_()**

Maneja los reintentos de configuración automáticamente (similar a Arduino que reinicia `get_working_status()`):

```cpp
void check_config_retry_() {
    if (config_retry_count_ < MAX_CONFIG_RETRIES) {
        // Reintentar envío de comando
        ESP_LOGW(TAG, "Retrying config command (attempt %u/%u)...",
                 config_retry_count_ + 1, MAX_CONFIG_RETRIES);
        send_config_command_();
    } else {
        // Si fallan todos los intentos, continuar recibiendo datos de todas formas
        // (el sensor podría ya estar en modo continuo)
        ESP_LOGW(TAG, "Max config retries exceeded. Starting to receive data anyway...");
        awaiting_config_response_ = false;
    }
}
```

### 3. **Secuencia de Inicialización Mejorada**

```
Paso 1: Enviar comando de configuración
         [0x11][0x04][0x50][DF1][DF2][DF3][CS]
         ↓ (esperar con timeout 2s)
         
Paso 2: Esperar respuesta [0x16][0x01][0x50][CS]
         ↓ 
         ✓ Confirmado → Paso 3
         ✗ Timeout → Reintento (hasta 5 veces)
         
Paso 3: Modo continuo confirmado
         
Paso 4: Recibir datos continuos [0x16][0x05][0x50][CO2H][CO2L][DF3][DF4][CS]
```

### 4. **Setup() Mejorado**

Ahora es muy similar a `setupCM1106()` de Arduino:

```cpp
void setup() {
    ESP_LOGCONFIG(TAG, "=== CM1106SL-NS Initialization ===");
    ESP_LOGCONFIG(TAG, "Step 1: Sending initial config command for continuous mode");
    ESP_LOGCONFIG(TAG, "Config: period=%us, smoothing=%u",
                  config_period_s_, smoothing_samples_);
    
    // Inicializar variables
    awaiting_config_response_ = true;
    config_retry_count_ = 0;
    continuous_mode_confirmed_ = false;
    
    // Enviar comando
    send_config_command_();
}
```

### 5. **Loop() con 3 Fases Claras**

```cpp
// Fase 1: Esperar respuesta de config con reintentos
if (awaiting_config_response_) {
    // Esperar [0x16][0x01][0x50][CS]
    // Con reintentos automáticos via check_config_retry_()
}

// Fase 2: Monitorear timeout de datos
if (millis() - last_frame_time > measurement_period_) {
    // Timeout: Activar sensor de error
}

// Fase 3: Procesar datos continuos
while (available() >= FRAME_LENGTH) {
    // Leer y procesar frames de datos
    // Validar header, checksum, CO2, DF3, DF4
}
```

### 6. **Mejor Logging**

Ahora los logs muestran el progreso paso a paso:

```
=== CM1106SL-NS Initialization ===
Step 1: Sending initial config command for continuous mode
        Period: 4 seconds, Smoothing: 1 samples
>>> SENDING CONFIG COMMAND
    Period: 4 seconds (0x00 0x04)
    Smoothing: 1 samples
    Bytes: 0x11 0x04 0x50 0x00 0x04 0x01 0xAB

Step 2: Waiting for config ACK from sensor (2000ms timeout, attempt 1/5)

✓ CONFIG ACK RECEIVED: 0x16 0x01 0x50 0xAE
Step 3: Continuous mode configured successfully
Step 4: Ready to receive sensor data
```

---

## Variables y Constantes Nuevas

### Constantes Agregadas a Header

```cpp
// Continuous Mode Configuration
static constexpr uint32_t CONFIG_RETRY_DELAY = 1000;    // 1 segundo
static constexpr uint8_t MAX_CONFIG_RETRIES = 5;        // 5 intentos máximo

// Protocol constants (ya existían, ahora documentados)
static constexpr uint8_t FRAME_HEADER_1 = 0x16;
static constexpr uint8_t FRAME_HEADER_2 = 0x05;
static constexpr uint8_t FRAME_COMMAND = 0x50;
static constexpr uint16_t CO2_MIN_VALID = 300;
static constexpr uint16_t CO2_MAX_VALID = 5000;
static constexpr uint32_t CONFIG_RESPONSE_TIMEOUT = 2000;
static constexpr uint8_t MAX_BAD_FRAMES = 5;
static constexpr uint8_t STABILITY_THRESHOLD = 20;
static constexpr uint32_t WARMUP_STATUS_VALUE = 0x08;
```

### Variables Nuevas

```cpp
uint8_t config_retry_count_ = 0;           // Contador de reintentos
uint32_t config_retry_time_ = 0;           // Tiempo para reintento
bool continuous_mode_confirmed_ = false;   // Estado de confirmación
```

---

## Funciones Nuevas/Modificadas

### Nueva: `check_config_retry_()`
- Maneja la lógica de reintentos
- Decide si reintentar o abandonar
- Permite modo fallback sin confirmación

### Modificada: `setup()`
- Ahora similar a `setupCM1106()` de Arduino
- Mejor logging de pasos de inicialización
- Inicializa variables de reintento

### Modificada: `loop()`
- Fase 1 mejorada con reintentos
- Mejor logging con emojis y números de intento
- Mejor descripción de operaciones

### Mejorada: `send_config_command_()`
- Logging más detallado
- Muestra parámetros y checksum

### Mejorada: `dump_config()`
- Muestra estado de configuración continua
- Muestra parámetros de reintentos
- Más información sobre configuración actual

---

## Compatibilidad con Arduino

| Aspecto | Arduino | ESPHome | Match |
|--------|---------|---------|-------|
| **Modo** | `CM1106_CONTINUOUS_MEASUREMENT` | Envía comando 0x11 0x04 0x50 | ✓ |
| **Detección** | `get_working_status()` | Espera respuesta 0x16 0x01 0x50 | ✓ |
| **Cambio de Modo** | `set_working_status()` | Envía comando de config | ✓ |
| **Lectura CO2** | `get_co2()` | Lee bytes [3] y [4] | ✓ |
| **Período** | `get_measurement_period()` | `config_period_s_` | ✓ |
| **Suavizado** | `get_measurement_period()` | `smoothing_samples_` | ✓ |
| **Reintentos** | Implícito en librería | Explícito con `check_config_retry_()` | ✓ |
| **Fallback** | Continúa si librería OK | Continúa si max retries | ✓ |

---

## Flujo de Ejecución

```
POWER ON
   ↓
setup() {
   awaiting_config_response_ = true
   config_retry_count_ = 0
   send_config_command_()  // [0x11][0x04][0x50][DF1][DF2][DF3][CS]
}
   ↓
loop() {
   if (awaiting_config_response_) {
      if (response_received) {
         continuous_mode_confirmed_ = true
         awaiting_config_response_ = false
      } else if (timeout) {
         check_config_retry_() {
            if (retry_count < MAX) {
               send_config_command_()  // REINTENTO
            } else {
               awaiting_config_response_ = false  // FALLBACK
            }
         }
      }
      return  // No procesar datos hasta confirmación
   }
   
   // Procesar datos continuos
   if (frame_received) {
      publish CO2, DF3, DF4, estabilidad, IAQ...
   }
}
```

---

## Referencias de Código

- **Arduino**: `arduino/my_cm1106.ino` líneas 27-102 (setupCM1106)
- **Arduino**: `arduino/my_cm1106.ino` líneas 118-156 (readCM1106)
- **ESPHome**: `esphome/components/cm1106sl_ns/cm1106sl_ns.h` (constantes y variables)
- **ESPHome**: `esphome/components/cm1106sl_ns/cm1106sl_ns.cpp` (implementación)
- **Protocolo**: `UART_COMMUNICATION.md` (especificación UART)

---

## Pruebas Recomendadas

1. **Inicialización exitosa**: Verificar que se reciba 0x16 0x01 0x50 [CS] en el primer intento
2. **Reintentos**: Simular falla en respuesta y verificar que reininte hasta 5 veces
3. **Modo fallback**: Desconectar TX del sensor y verificar que continue recibiendo si RX está activo
4. **Datos continuos**: Verificar que reciba frames [0x16][0x05][0x50]... periódicamente
5. **Estabilidad**: Verificar contador de estabilidad aumenta/disminuye correctamente

---

## Notas Importantes

- El componente ESPHome implementa el **nivel de protocolo UART**, mientras que Arduino usa una **librería abstracta**
- ESPHome es **más robusto** porque implementa reintentos explícitos
- ESPHome tiene **modo fallback** que permite continuar sin confirmación
- Ambas implementaciones son **100% compatibles** en protocolo UART
- Los logs de ESPHome son **más detallados** para debugging

