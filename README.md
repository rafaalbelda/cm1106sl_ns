# cm1106sl_ns
cm1106sl ns sensor in UART mode for ESPHome

## Control de la página LVGL desde Home Assistant

La página mostrada en la pantalla puede controlarse desde Home Assistant mediante
un `select` template de ESPHome. Cada opción del selector se asocia con una de
las páginas LVGL definidas en `aqi32-w-vertical.yaml`.

```yaml
select:
  - platform: template
    name: "Página de pantalla"
    id: display_page
    icon: "mdi:view-dashboard"
    optimistic: true
    initial_option: "Calidad del aire"
    options:
      - "Calidad del aire"
      - "Resumen"
      - "Ventilación"
      - "Partículas"
      - "CO2"
      - "VOC y NOx"
      - "Comparación AQI"
      - "Tiempo"
    set_action:
      - if:
          condition:
            lambda: 'return x == "Calidad del aire";'
          then:
            - lvgl.page.show:
                id: lvgl_page1
                animation: OUT_RIGHT
                time: 300ms
      - if:
          condition:
            lambda: 'return x == "Resumen";'
          then:
            - lvgl.page.show:
                id: lvgl_page_overview
                animation: OUT_LEFT
                time: 300ms
      - if:
          condition:
            lambda: 'return x == "Ventilación";'
          then:
            - lvgl.page.show:
                id: lvgl_page_rows
                animation: OUT_LEFT
                time: 300ms
      - if:
          condition:
            lambda: 'return x == "Partículas";'
          then:
            - lvgl.page.show:
                id: lvgl_page2
                animation: OUT_LEFT
                time: 300ms
      - if:
          condition:
            lambda: 'return x == "CO2";'
          then:
            - lvgl.page.show:
                id: lvgl_page3
                animation: OUT_LEFT
                time: 300ms
      - if:
          condition:
            lambda: 'return x == "VOC y NOx";'
          then:
            - lvgl.page.show:
                id: lvgl_page4
                animation: OUT_LEFT
                time: 300ms
      - if:
          condition:
            lambda: 'return x == "Comparación AQI";'
          then:
            - lvgl.page.show:
                id: lvgl_page_aqi_compare
                animation: OUT_LEFT
                time: 300ms
      - if:
          condition:
            lambda: 'return x == "Tiempo";'
          then:
            - lvgl.page.show:
                id: lvgl_page5
                animation: OUT_LEFT
                time: 300ms
```

Después de instalar el firmware, Home Assistant creará una entidad similar a
`select.airq32_w_pagina_de_pantalla`. Se puede usar desde el panel o desde una
automatización:

```yaml
action:
  - action: select.select_option
    target:
      entity_id: select.airq32_w_pagina_de_pantalla
    data:
      option: "Tiempo"
```

Para que el botón físico y el botón template mantengan sincronizado el estado
del selector, deben avanzar mediante `select.next` en lugar de llamar
directamente a `lvgl.page.next`:

```yaml
on_press:
  then:
    - select.next:
        id: display_page
        cycle: true
```

El orden de `options` debe coincidir con el orden de las páginas LVGL para que
la navegación cíclica del botón sea coherente con el selector de Home Assistant.

Referencias: [LVGL en ESPHome](https://esphome.io/components/lvgl/) y
[componente Select](https://esphome.io/components/select/).

## Cambiar entre el firmware horizontal y el vertical

La orientación no se puede cambiar solamente con una entidad de Home Assistant,
porque cada diseño LVGL y las dimensiones de la pantalla se compilan dentro del
firmware. Para cambiarla hay que instalar por OTA el firmware horizontal o el
vertical.

### Opción recomendada: botones en Home Assistant

ESPHome puede descargar e instalar un firmware desde un servidor HTTP. Para que
sea posible cambiar en ambos sentidos, hay que añadir la siguiente configuración
tanto a `aqi32-w.yaml` como a `aqi32-w-vertical.yaml`:

```yaml
http_request:

ota:
  - platform: esphome
    password: !secret airq32_w_ota_password
  - platform: http_request

button:
  - platform: template
    name: "Instalar pantalla horizontal"
    icon: "mdi:monitor"
    on_press:
      - ota.http_request.flash:
          url: http://SERVIDOR/firmware-horizontal.ota.bin
          md5_url: http://SERVIDOR/firmware-horizontal.md5

  - platform: template
    name: "Instalar pantalla vertical"
    icon: "mdi:cellphone"
    on_press:
      - ota.http_request.flash:
          url: http://SERVIDOR/firmware-vertical.ota.bin
          md5_url: http://SERVIDOR/firmware-vertical.md5
```

Hay que sustituir `SERVIDOR` por la IP o el nombre de un servidor HTTP accesible
desde el ESP32. En ese servidor deben publicarse los dos archivos
`firmware.ota.bin` generados al compilar las configuraciones y sus archivos MD5.
Se debe utilizar el binario OTA, no `firmware.factory.bin`.

#### Generar los BIN y MD5

Como las dos configuraciones utilizan el mismo nombre de dispositivo,
`airq32-w`, ESPHome escribe sus resultados en el mismo directorio. Por eso hay
que copiar y renombrar el BIN horizontal antes de compilar el vertical.

Desde PowerShell, situado en el directorio del repositorio:

```powershell
esphome compile aqi32-w.yaml

Copy-Item `
  ".esphome/build/airq32-w/.pioenvs/airq32-w/firmware.ota.bin" `
  "firmware-horizontal.ota.bin"

esphome compile aqi32-w-vertical.yaml

Copy-Item `
  ".esphome/build/airq32-w/.pioenvs/airq32-w/firmware.ota.bin" `
  "firmware-vertical.ota.bin"
```

A continuación se generan los archivos de comprobación MD5:

```powershell
(Get-FileHash "firmware-horizontal.ota.bin" -Algorithm MD5).Hash.ToLower() |
  Out-File "firmware-horizontal.md5" -Encoding ASCII

(Get-FileHash "firmware-vertical.ota.bin" -Algorithm MD5).Hash.ToLower() |
  Out-File "firmware-vertical.md5" -Encoding ASCII
```

El resultado son los cuatro archivos que deben publicarse en el servidor:

```text
firmware-horizontal.ota.bin
firmware-horizontal.md5
firmware-vertical.ota.bin
firmware-vertical.md5
```

El archivo MD5 debe contener únicamente los 32 caracteres hexadecimales en
minúsculas correspondientes al BIN asociado.

También se pueden compilar desde el panel de ESPHome. Para cada configuración se
selecciona `Install`, descarga manual y formato **OTA** (anteriormente llamado
**Legacy**). Después se renombra el archivo descargado y se genera su MD5 con los
comandos anteriores. El formato **Factory** no sirve para esta actualización.

#### Alojar los firmwares en Home Assistant

Home Assistant puede actuar como servidor de los archivos. Se pueden guardar en
un subdirectorio de `/config/www/`, por ejemplo:

```text
/config/www/firmware/firmware-horizontal.ota.bin
/config/www/firmware/firmware-horizontal.md5
/config/www/firmware/firmware-vertical.ota.bin
/config/www/firmware/firmware-vertical.md5
```

El contenido de `/config/www/` se publica mediante la ruta `/local/`. Si la IP
local de Home Assistant es `192.168.1.250`, las acciones quedarían así:

```yaml
- ota.http_request.flash:
    url: http://192.168.1.250:8123/local/firmware/firmware-horizontal.ota.bin
    md5_url: http://192.168.1.250:8123/local/firmware/firmware-horizontal.md5

- ota.http_request.flash:
    url: http://192.168.1.250:8123/local/firmware/firmware-vertical.ota.bin
    md5_url: http://192.168.1.250:8123/local/firmware/firmware-vertical.md5
```

La dirección debe ser accesible directamente desde el ESP32. Conviene comprobar
las URL desde otro equipo de la misma red antes de actualizar. La ruta `/local/`
no debe utilizarse para guardar secretos, ya que sus archivos se sirven sin la
autenticación habitual de Home Assistant. Si se acaba de crear `/config/www/`,
puede ser necesario reiniciar Home Assistant.

Cuando Home Assistant utiliza HTTPS con un certificado que el ESP32 no reconoce,
la descarga puede fallar. En una red local aislada suele ser más sencillo usar
la dirección HTTP local de Home Assistant.

Una vez instalado cualquiera de los dos firmwares, Home Assistant mostrará los
dos botones. Al pulsar uno, el ESP32 descargará el binario correspondiente, lo
instalará y se reiniciará con la nueva orientación. Es recomendable colocar los
botones en una tarjeta que solicite confirmación para evitar actualizaciones
accidentales:

```yaml
type: button
entity: button.airq32_w_instalar_pantalla_vertical
name: Instalar pantalla vertical
tap_action:
  action: toggle
  confirmation:
    text: ¿Instalar el firmware vertical y reiniciar la pantalla?
```

No se debe desconectar la alimentación durante la actualización. Además, ambos
firmwares deben conservar `ota.http_request` y los dos botones; si uno de ellos
no los incluye, después de instalarlo no se podrá regresar al otro desde Home
Assistant.

### Opción manual

También se puede compilar y subir cada configuración desde el panel de ESPHome o
desde la línea de comandos, indicando la dirección del dispositivo:

```bash
esphome run aqi32-w.yaml --device IP_DEL_ESP32
esphome run aqi32-w-vertical.yaml --device IP_DEL_ESP32
```

Referencia: [OTA mediante HTTP Request en ESPHome](https://esphome.io/components/ota/http_request/).
