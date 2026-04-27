# CM1106SL-NS - Informacion extraida del PDF

Fuente: `CM1106SL-NS.pdf`

- Producto: Super Low Power CO2 Sensor Module
- Referencia: CM1106SL-NS
- Version del documento: V0.7
- Fecha: 2021-11-05
- Fabricante/contacto: Cubic Sensor and Instrument Co., Ltd. / info@gassensor.com.cn

## Descripcion general

Modulo sensor de CO2 NDIR de muy bajo consumo, pensado para equipos a bateria, HVAC e instrumentos portatiles. Mide CO2 en aire interior usando tecnologia infrarroja no dispersiva.

Caracteristicas principales:

- Tecnologia NDIR.
- Bajo consumo.
- Alta estabilidad a largo plazo y vida util >= 15 anos.
- Compensacion/calibracion de temperatura en el rango de medicion.
- Autocalibracion ABC configurable por el host.
- Ciclo de medicion ajustable por el host.
- Salidas UART TTL e I2C.

## Especificaciones

| Parametro | Valor |
| --- | --- |
| Gas objetivo | CO2 |
| Principio | NDIR |
| Rango de medida | 0-5000 ppm |
| Temperatura de trabajo | -10 a 50 C |
| Humedad de trabajo | 0-95 %RH, sin condensacion |
| Temperatura de almacenamiento | -30 a 70 C |
| Humedad de almacenamiento | 0-95 %RH, sin condensacion |
| Precision | +/- (50 ppm + 5 % lectura), ver notas |
| Precision en modo single con media >= 5 | +/- (50 ppm + 3 % lectura), 400-2000 ppm |
| Dependencia de presion | 1 % lectura / kPa @ 80-106 kPa |
| Alimentacion | DC 3.3-5.5 V |
| Corriente media | <= 74 uA con ciclo 1 min; <= 37 uA con ciclo 2 min |
| Modo por defecto | Medicion unica / working mode A |
| Dimensiones | 33.5 x 19.7 x 9.1 mm |
| Peso | 5 g |
| Vida util | >= 15 anos |

Notas importantes:

- El sensor esta disenado para 0-5000 ppm, pero exponerlo a concentraciones por debajo de 400 ppm puede afectar al algoritmo ABC si esta activado.
- En aplicaciones IAQ normales, la precision se define en 10-35 C y 0-85 %RH.
- En modo continuo, el datasheet indica precision con alimentacion continua, ciclo de 4 s y media movil sobre 24 datos.
- En modo single, la salida no incluye media movil; el host debe promediar si quiere mas estabilidad.

## Conectores y pines

### CON5

| Pin | Nombre | Descripcion |
| --- | --- | --- |
| 1 | GND | Tierra |
| 2 | VBB | Alimentacion 3.3-5.5 V |
| 3 | VDDIO | Alimentacion nivel logico comunicacion |
| 4 | RX/SDA | UART RX / I2C SDA |
| 5 | TX/SCL | UART TX / I2C SCL |

### CON4

| Pin | Nombre | Descripcion |
| --- | --- | --- |
| 1 | EN | Enable de alimentacion: alto = activo, bajo = apagado |
| 2 | DVCC | Salida de alimentacion 2.8 V |
| 3 | RDY | Datos listos; salida activa a nivel bajo, 2.8 V |
| 4 | COMSEL | Seleccion comunicacion: alto/flotante = UART, bajo = I2C |

Reglas de alimentacion:

- VBB 3.3-5.5 V y EN alto: sensor funcionando.
- EN bajo o VBB sin alimentacion: sensor apagado.
- El nivel electrico de comunicacion sigue a `VDDIO`.

## Modos de trabajo

### Working mode A: single measurement

Modo por defecto. El host controla la alimentacion mediante `EN` o `VBB`.

Secuencia indicada:

1. Poner `VBB` y `EN` en alto.
2. Esperar unos 500 ms.
3. La fuente IR trabaja unos 100 ms.
4. El MCU calcula durante unos 100 ms.
5. Contemplar unos 30 ms para comunicacion.
6. Tiempo total aproximado del ciclo: 730 ms.
7. Cuando `RDY` pasa a bajo, el host puede comunicarse.
8. Tras leer, poner `EN` a bajo para ahorrar energia.

En este modo la lectura no viene promediada. El host puede aplicar media movil.

### Working mode B: continuous measurement

El sensor mide periodicamente sin que el host dispare cada medicion. El intervalo por defecto es 2 minutos. El host puede configurar:

- `T`: intervalo de trabajo.
- `D`: numero de datos para media movil, maximo 120.

## Calibracion ABC

ABC esta desactivado por defecto.

En modo continuo:

- El sensor debe estar alimentado durante todo el ciclo ABC.
- Ciclo por defecto: 7 dias.
- Usa el minimo de CO2 detectado durante el ciclo como baseline de 400 ppm.

En modo single:

- El sensor no acumula tiempo de funcionamiento continuo.
- El host debe enviar el comando de almacenamiento ABC.
- Con ciclo por defecto de 7 dias, calibra tras `7 * 48 = 336` mediciones almacenadas.

Requisitos:

- Asegurar que durante el ciclo el ambiente alcanza aire fresco exterior, aproximadamente 400 ppm.
- Si no se puede garantizar, se recomienda calibracion manual periodica.

## UART

Configuracion fisica:

| Parametro | Valor |
| --- | --- |
| Baud rate | 9600 |
| Data bits | 8 |
| Stop bits | 1 |
| Paridad | No |
| Flow control | No |

Formato:

```text
HEAD LEN CMD DATA... CS
```

- Comandos enviados por host: `HEAD = 0x11`.
- Respuestas del modulo: `HEAD = 0x16`.
- `LEN = longitud de CMD + DATA`.
- Checksum: `CS = 256 - ((HEAD + LEN + CMD + DATA...) % 256)`.

### Comandos UART principales

| Funcion | Comando |
| --- | --- |
| Leer CO2 | `0x01` |
| Calibrar CO2 | `0x03` |
| Leer parametros ABC | `0x0F` |
| Configurar ABC | `0x10` |
| Leer version software | `0x1E` |
| Leer numero de serie | `0x1F` |
| Set/check periodo y suavizado | `0x50` |
| Set/check modo de trabajo | `0x51` |
| Almacenar dato para ABC en single mode | `0x11` |

### Leer CO2

```text
Send:     11 01 01 ED
Response: 16 05 01 DF1 DF2 DF3 DF4 CS
```

Calculo:

```text
CO2 ppm = DF1 * 256 + DF2
```

Ejemplo:

```text
Response: 16 05 01 02 58 00 00 8A
CO2 = 0x02 * 256 + 0x58 = 600 ppm
```

### Calibracion de CO2

```text
Send:     11 03 03 DF1 DF2 CS
Response: 16 01 03 CS
```

- Objetivo: `DF1 * 256 + DF2`, rango 400-1500 ppm.
- Antes de calibrar, mantener el ambiente en el valor objetivo durante 2 minutos.
- Enviar tras al menos 5 s desde power-on con `VBB` y `EN` altos.

Ejemplo para 600 ppm:

```text
11 03 03 02 58 8F
```

### Leer parametros ABC

```text
Send:     11 01 0F DF
Response: 16 07 0F DF1 DF2 DF3 DF4 DF5 DF6 CS
```

Campos:

| Campo | Descripcion |
| --- | --- |
| DF1 | Reservado, default `0x64` |
| DF2 | ABC: `0` abierto/activo, `2` cerrado/inactivo |
| DF3 | Ciclo de calibracion, 1-10 dias, default 7 |
| DF4 DF5 | Baseline: `DF4 * 256 + DF5`, default 400 ppm (`01 90`) |
| DF6 | Reservado, default `0x64` |

### Configurar ABC

```text
Send:     11 07 10 DF1 DF2 DF3 DF4 DF5 DF6 CS
Response: 16 01 10 D9
```

Abrir ABC con ciclo 7 dias y baseline 400 ppm:

```text
11 07 10 64 00 07 01 90 64 78
```

Cerrar ABC:

```text
11 07 10 64 02 07 01 90 64 76
```

### Leer version software

```text
Send:     11 01 1E D0
Response: 16 0C 1E CH1 ... CH11 CS
```

Los bytes `CHx` son ASCII.

### Leer numero de serie

```text
Send:     11 01 1F CF
Response: 16 0B 1F SN1 SN2 SN3 SN4 SN5 CS
```

Cada bloque `SNn` va de 0 a 9999; juntos forman el numero de serie de 20 digitos.

### Configurar o consultar periodo y suavizado

Set:

```text
Send:     11 04 50 DF1 DF2 DF3 CS
Response: 16 01 50 CS
```

Check:

```text
Send:     11 01 50 CS
Response: 16 04 50 DF1 DF2 DF3 CS
```

Campos:

- Periodo en segundos: `DF1 * 256 + DF2`.
- Rango UART indicado: 4 s a 10 min.
- Numero de datos suavizados: `DF3`.
- El tiempo total de suavizado no debe superar 30 min.

Ejemplo para 2 min y 15 muestras:

```text
11 04 50 00 78 0F 14
```

### Configurar o consultar modo de trabajo

Set:

```text
Send:     11 02 51 DF1 CS
Response: 16 01 51 CS
```

Check:

```text
Send:     11 01 51 CS
Response: 16 02 51 DF1 CS
```

Valores:

- `DF1 = 0`: single measurement mode / working mode A.
- `DF1 = 1`: continuous measurement mode / working mode B.

Nota: el ejemplo del PDF para pasar a continuo aparece como `11 01 51 01 98`, aunque por el formato descrito de `Set` seria esperable longitud `02`. Conviene validar con el sensor real.

### Almacenar dato ABC en modo single

```text
Send:     11 01 11 CS
Response: 16 01 11 CS
```

Uso recomendado:

- En modo single, enviar cada 30 min si el intervalo de alimentacion es menor de 30 min.
- Si el intervalo es mayor, enviarlo tras cada medicion.

## I2C

El sensor trabaja como esclavo I2C.

| Parametro | Valor |
| --- | --- |
| Velocidad | Hasta 100 kbit/s |
| Direccionamiento | 7 bit |
| Direccion original/default | `0x34` |
| Write address | `0x68` |
| Read address | `0x69` |
| Clock stretch | Si |
| Pull-up SCL/SDA | 10 kohm |
| Escritura EEPROM | < 25 ms |

### Registros de solo lectura

| Registro | Funcion |
| --- | --- |
| `0x00-0x01` | Error status |
| `0x06-0x07` | CO2 ppm: `0x06 * 256 + 0x07` |
| `0x08-0x09` | Temperatura chip en C x100 |
| `0x0D` | Contador de mediciones, 0-255 |
| `0x0E-0x0F` | Tiempo dentro del ciclo actual, incrementa cada 2 s |
| `0x10-0x15` | Valores adicionales de CO2 |

Bits relevantes de error:

- Bit 0: error fatal de inicializacion analogica.
- Bit 5: fuera de rango.

### Registros lectura/escritura

| Registro | Funcion |
| --- | --- |
| `0x81` | Estado calibracion, solo lectura |
| `0x82-0x83` | Comando de calibracion |
| `0x84-0x85` | Target de calibracion |
| `0x86-0x87` | Override valor CO2, default 32767 |
| `0x88-0x89` | Tiempo desde ultima ABC en medias horas |
| `0x93` | Iniciar medicion single escribiendo `1` |
| `0x95` | Modo: `0` continuo, `1` single default |
| `0x96-0x97` | Periodo de medicion en segundos, 2-65534 |
| `0x9A-0x9B` | Periodo ABC en horas, 24-240; default 168 |
| `0x9E-0x9F` | Target ABC/background, default 400 ppm |
| `0xA5` | Control ABC: bit 1, `0` enabled, `1` disabled |
| `0xA7` | Direccion I2C 7 bit, default `0x34` |

Comandos de calibracion I2C:

| Comando | Funcion |
| --- | --- |
| `0x7C05` | Target calibration |
| `0x7C06` | Background calibration |
| `0x7C07` | Zero calibration |

Ejemplo lectura CO2 en placa de pruebas Cubic:

```text
Send:     68 06 00 02
Response: sin respuesta
Send:     69
Response: 04 EE
CO2 = 0x04EE = 1262 ppm
```

Ejemplo configurar modo continuo:

```text
Send: 68 95 00 01 01
```

## Instalacion

- Dejar al menos 1.5 mm entre el filtro impermeable del sensor y otros componentes para permitir difusion de aire.
- Se recomienda soldadura manual para reducir esfuerzos mecanicos sobre el sensor.

## Embalaje

- 75 sensores por bandeja.
- 15 bandejas por carton.
- 1125 sensores por carton.
- Dimensiones carton: W395 x L310 x H200 mm.
- Material: PS antiestatico.
