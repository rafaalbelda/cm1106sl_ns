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
