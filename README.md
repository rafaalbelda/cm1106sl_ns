# cm1106sl_ns
cm1106sl ns sensor in UART mode for ESPHome

## Versiones standalone

Las variantes standalone mantienen la pantalla LVGL y los sensores locales, pero
no necesitan entidades meteorológicas de Home Assistant. El ESP32 consulta
directamente la predicción horaria de AEMET y sirve un panel web autocontenido;
el CSS y JavaScript se incluyen dentro del firmware y no se descargan de
Internet. Los sensores locales y el panel siguen funcionando sin salida a
Internet, pero la predicción necesita que el ESP32 pueda acceder por HTTPS a
`opendata.aemet.es`.

| Configuración | Orientación | Fuente meteorológica | Panel web local |
| --- | --- | --- | --- |
| `aqi32-w-standalone.yaml` | Horizontal | AEMET OpenData | Sí |
| `aqi32-w-vertical-standalone.yaml` | Vertical | AEMET OpenData | Sí |

La API nativa de ESPHome continúa habilitada para administración y OTA, pero
Home Assistant es opcional para el funcionamiento diario de estas variantes.

### Archivos necesarios

Al copiar una configuración standalone al directorio de ESPHome hay que
conservar también estos archivos y rutas relativas:

```text
aqi32-w-standalone.yaml                  # o la variante vertical
aemet-weather.yaml
standalone-settings.yaml
web/airq32-dashboard.css
web/airq32-dashboard.js
secrets.yaml
```

`aemet-weather.yaml` implementa las dos peticiones requeridas por AEMET, procesa
la respuesta y publica temperatura, sensación térmica, máxima, mínima,
humedad, probabilidad de precipitación y estado del cielo. La consulta se hace
al conectar el Wi-Fi, se repite inicialmente cada cinco minutos y también puede
iniciarse con el botón **Actualizar AEMET** de la interfaz web o de ESPHome.
El intervalo puede cambiarse entre 1 y 1440 minutos desde **Configuración →
Intervalo de AEMET** y queda guardado entre reinicios.

### Secrets y clave AEMET

Además de los secrets habituales del dispositivo, las versiones standalone
necesitan `aemet_api_key`:

```yaml
wifi_ssid: "MI_WIFI"
wifi_password: "MI_PASSWORD"
airq32_w_api_encryption_key: "CLAVE_API_ESPHOME"
airq32_w_ota_password: "PASSWORD_OTA_Y_WEB"
airq32_w_fallback_password: "PASSWORD_AP"
aemet_api_key: "CLAVE_DE_AEMET_OPENDATA"
```

Puede copiarse la entrada de `aemet-secrets.example.yaml`. La clave se solicita
en [AEMET OpenData](https://opendata.aemet.es/). El valor de `secrets.yaml` se
usa como clave inicial al instalar el firmware. Después puede reemplazarse en
**Configuración → Clave API de AEMET** dentro del panel web; el cambio queda
guardado en la memoria flash, sobrevive a los reinicios e inicia una consulta
meteorológica inmediatamente. Por seguridad, el formulario nunca muestra la
clave actual.

La clave no debe incorporarse al YAML principal, al repositorio, a capturas ni
a registros compartidos. El panel usa HTTP local, por lo que la clave solo debe
cambiarse desde una red de confianza. Si una clave se expone, debe revocarse y
reemplazarse.

### Compilar e instalar

Desde el directorio que contiene todos los archivos anteriores:

```bash
esphome run aqi32-w-standalone.yaml
```

Para la orientación vertical:

```bash
esphome run aqi32-w-vertical-standalone.yaml
```

Ambas configuraciones usan el nombre de nodo `airq32-w`, por lo que debe
instalarse solo una orientación cada vez. Para cambiar entre ellas hay que
compilar e instalar el firmware correspondiente.

### Panel web local

Una vez conectado el dispositivo, el panel está disponible en:

```text
http://airq32-w.local/
```

También puede utilizarse `http://IP_DEL_ESP32/`. El usuario es `admin` y la
contraseña es el valor de `airq32_w_ota_password`. La autenticación es Digest.
El panel permite consultar los sensores interiores y la predicción AEMET,
cambiar la página de la pantalla, ajustar el brillo y el offset de temperatura,
cambiar la clave y el intervalo de AEMET, configurar una nueva conexión Wi-Fi y
forzar una actualización meteorológica.

### Cambiar la conexión Wi-Fi

En **Configuración → Conexión Wi-Fi** se puede introducir un nuevo SSID y su
contraseña. Al pulsar **Conectar y guardar**, el ESP32 intenta conectarse durante
30 segundos y guarda las credenciales en memoria persistente. La página dejará
de responder mientras cambia de red; después debe abrirse de nuevo mediante
`http://airq32-w.local/` o usando la dirección asignada por el nuevo router.

Si la conexión falla, el dispositivo conserva el punto de acceso de recuperación
**Airq32-W Fallback Hotspot**. Tras aproximadamente un minuto sin conexión se
puede acceder a `http://192.168.4.1/` para corregir las credenciales mediante el
portal cautivo. Como la configuración se envía por HTTP local, debe realizarse
únicamente desde una red de confianza.

### Diagnóstico de AEMET

Las entidades **AEMET Estado** y **AEMET Última actualización** aparecen en
el panel web. El flujo normal es `Consultando`, `Descargando` y `Actualizado`.
Si no aparecen datos, hay que revisar primero ese estado:

- `Error de acceso`: la clave no es válida o AEMET ha rechazado la petición.
- `Error de conexión`: el ESP32 no ha podido resolver el servidor o establecer
  la conexión HTTPS.
- `Error de descarga`: la URL temporal de datos devolvió un error HTTP.
- `Límite AEMET (429)`: se hicieron varias consultas en poco tiempo; hay que
  esperar al menos un minuto antes de volver a intentarlo.
- `JSON: ...`: la respuesta no pudo procesarse; el texto posterior indica el
  error concreto de ArduinoJson.
- `Sin predicción`: AEMET respondió correctamente, pero no incluyó datos para
  el municipio configurado.

El municipio se configura mediante la sustitución `aemet_municipality` de
`aemet-weather.yaml`; el valor incluido, `28903`, corresponde a Tres Cantos y se
usa como valor inicial.

La localidad puede cambiarse desde **Configuración → Localidad de AEMET**
introduciendo su nombre oficial o el código INE de cinco cifras. El ESP32
consulta `https://opendata.aemet.es/opendata/api/maestro/municipios`, recorre el
catálogo sin cargarlo completo en memoria, guarda la coincidencia y actualiza la
predicción. La búsqueda por nombre ignora mayúsculas, espacios, signos y los
acentos españoles habituales. Si existen varios municipios con el mismo nombre,
el panel solicita el código INE para evitar elegir una provincia incorrecta. La
pantalla meteorológica muestra el nombre de la localidad seleccionada en lugar
del nombre del proveedor AEMET.

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
