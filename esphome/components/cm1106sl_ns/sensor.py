import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    UNIT_PARTS_PER_MILLION,
    ICON_MOLECULE_CO2,
    DEVICE_CLASS_CARBON_DIOXIDE,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]

cm1106sl_ns_ns = cg.global_ns.namespace("cm1106sl_ns")
CM1106SLNS = cm1106sl_ns_ns.class_("CM1106SLNS", cg.PollingComponent, uart.UARTDevice)

CONF_UART_ID = "uart_id"

# IDs opcionales para sensores secundarios
CONF_ID_DF3 = "id_df3"
CONF_ID_DF4 = "id_df4"
CONF_ID_STATUS = "id_status"
CONF_ID_STABILITY = "id_stability"
CONF_ID_READY = "id_ready"
CONF_ID_ERROR = "id_error"
CONF_ID_IAQ_NUM = "id_iaq_num"
CONF_ID_IAQ_TEXT = "id_iaq_text"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CM1106SLNS),

        # ID del sensor principal (obligatorio)
        cv.Required(CONF_ID): cv.declare_id(sensor.Sensor),

        cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
        cv.Required(CONF_NAME): cv.string,

        # IDs opcionales
        cv.Optional(CONF_ID_DF3): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ID_DF4): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ID_STATUS): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ID_STABILITY): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ID_READY): cv.declare_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_ID_ERROR): cv.declare_id(binary_sensor.BinarySensor),
        cv.Optional(CONF_ID_IAQ_NUM): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ID_IAQ_TEXT): cv.declare_id(text_sensor.TextSensor),
    }
).extend(cv.polling_component_schema("5s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], await cg.get_variable(config[CONF_UART_ID]))
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Sensor principal CO2
    co2 = await sensor.new_sensor(
        {
            CONF_ID: config[CONF_ID],
            CONF_NAME: config[CONF_NAME],
            "unit_of_measurement": UNIT_PARTS_PER_MILLION,
            "icon": ICON_MOLECULE_CO2,
            "device_class": DEVICE_CLASS_CARBON_DIOXIDE,
        }
    )
    cg.add(var.co2_sensor, co2)

    # DF3
    if CONF_ID_DF3 in config:
        df3 = await sensor.new_sensor({CONF_ID: config[CONF_ID_DF3], CONF_NAME: f"{config[CONF_NAME]} DF3"})
    else:
        df3 = await sensor.new_sensor({CONF_NAME: f"{config[CONF_NAME]} DF3"})
    cg.add(var.df3_sensor, df3)

    # DF4
    if CONF_ID_DF4 in config:
        df4 = await sensor.new_sensor({CONF_ID: config[CONF_ID_DF4], CONF_NAME: f"{config[CONF_NAME]} DF4"})
    else:
        df4 = await sensor.new_sensor({CONF_NAME: f"{config[CONF_NAME]} DF4"})
    cg.add(var.df4_sensor, df4)

    # Estado
    if CONF_ID_STATUS in config:
        status = await text_sensor.new_text_sensor({CONF_ID: config[CONF_ID_STATUS], CONF_NAME: f"{config[CONF_NAME]} Estado"})
    else:
        status = await text_sensor.new_text_sensor({CONF_NAME: f"{config[CONF_NAME]} Estado"})
    cg.add(var.status_sensor, status)

    # Estabilidad
    if CONF_ID_STABILITY in config:
        stability = await sensor.new_sensor({CONF_ID: config[CONF_ID_STABILITY], CONF_NAME: f"{config[CONF_NAME]} Estabilidad"})
    else:
        stability = await sensor.new_sensor({CONF_NAME: f"{config[CONF_NAME]} Estabilidad"})
    cg.add(var.stability_sensor, stability)

    # Ready
    if CONF_ID_READY in config:
        ready = await binary_sensor.new_binary_sensor({CONF_ID: config[CONF_ID_READY], CONF_NAME: f"{config[CONF_NAME]} Listo"})
    else:
        ready = await binary_sensor.new_binary_sensor({CONF_NAME: f"{config[CONF_NAME]} Listo"})
    cg.add(var.ready_sensor, ready)

    # Error
    if CONF_ID_ERROR in config:
        error = await binary_sensor.new_binary_sensor({CONF_ID: config[CONF_ID_ERROR], CONF_NAME: f"{config[CONF_NAME]} Error"})
    else:
        error = await binary_sensor.new_binary_sensor({CONF_NAME: f"{config[CONF_NAME]} Error"})
    cg.add(var.error_sensor, error)

    # IAQ numérico
    if CONF_ID_IAQ_NUM in config:
        iaq_num = await sensor.new_sensor({CONF_ID: config[CONF_ID_IAQ_NUM], CONF_NAME: f"{config[CONF_NAME]} IAQ (1-5)"})
    else:
        iaq_num = await sensor.new_sensor({CONF_NAME: f"{config[CONF_NAME]} IAQ (1-5)"})
    cg.add(var.iaq_numeric, iaq_num)

    # IAQ texto
    if CONF_ID_IAQ_TEXT in config:
        iaq_txt = await text_sensor.new_text_sensor({CONF_ID: config[CONF_ID_IAQ_TEXT], CONF_NAME: f"{config[CONF_NAME]} IAQ Texto"})
    else:
        iaq_txt = await text_sensor.new_text_sensor({CONF_NAME: f"{config[CONF_NAME]} IAQ Texto"})
    cg.add(var.iaq_text, iaq_txt)
