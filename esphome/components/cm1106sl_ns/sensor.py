import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor, text_sensor, binary_sensor
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    CONF_NAME,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["uart"]
#AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]

#cm1106sl_ns_ns = cg.global_ns.namespace("cm1106sl_ns")
cm1106sl_ns_ns = cg.esphome_ns.namespace("cm1106sl_ns")
CM1106SLNS = cm1106sl_ns_ns.class_("CM1106SLNS", cg.PollingComponent, uart.UARTDevice)

CONF_UART_ID = "uart_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CM1106SLNS),
            #cv.Required(CONF_UART_ID): cv.use_id(uart.UARTComponent),
            cv.Required(CONF_NAME): cv.string,
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        },
    ).extend(cv.polling_component_schema("60s"))
     .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config) -> None:
    #var = cg.new_Pvariable(config[CONF_ID], await cg.get_variable(config[CONF_UART_ID]))
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if co2_config := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2_config)
        cg.add(var.set_co2_sensor(sens))

    # CO2 principal
    co2 = await sensor.new_sensor(
        {
            CONF_ID: cg.declare_id(sensor.Sensor),
            CONF_NAME: config[CONF_NAME],
            "unit_of_measurement": UNIT_PARTS_PER_MILLION,
            "icon": ICON_MOLECULE_CO2,
            "device_class": DEVICE_CLASS_CARBON_DIOXIDE,
        }

    )
    cg.add(var.co2_sensor, co2)

    # DF3 / DF4
    df3 = await sensor.new_sensor({CONF_ID: cg.declare_id(sensor.Sensor), CONF_NAME: f"{config[CONF_NAME]} DF3"})
    df4 = await sensor.new_sensor({CONF_ID: cg.declare_id(sensor.Sensor), CONF_NAME: f"{config[CONF_NAME]} DF4"})
    cg.add(var.df3_sensor, df3)
    cg.add(var.df4_sensor, df4)

    # Estado (texto)
    status = await text_sensor.new_text_sensor(
        {CONF_ID: cg.declare_id(text_sensor.TextSensor), CONF_NAME: f"{config[CONF_NAME]} Estado"}
    )
    cg.add(var.status_sensor, status)

    # Estabilidad
    stability = await sensor.new_sensor(
        {CONF_ID: cg.declare_id(sensor.Sensor), CONF_NAME: f"{config[CONF_NAME]} Estabilidad"}
    )
    cg.add(var.stability_sensor, stability)

    # Listo / Error
    ready = await binary_sensor.new_binary_sensor(
        {CONF_ID: cg.declare_id(binary_sensor.BinarySensor), CONF_NAME: f"{config[CONF_NAME]} Listo"}
    )
    error = await binary_sensor.new_binary_sensor(
        {CONF_ID: cg.declare_id(binary_sensor.BinarySensor), CONF_NAME: f"{config[CONF_NAME]} Error"}
    )
    cg.add(var.ready_sensor, ready)
    cg.add(var.error_sensor, error)

    # IAQ numérico + texto
    iaq_num = await sensor.new_sensor(
        {CONF_ID: cg.declare_id(sensor.Sensor), CONF_NAME: f"{config[CONF_NAME]} IAQ (1-5)"}
    )
    iaq_txt = await text_sensor.new_text_sensor(
        {CONF_ID: cg.declare_id(text_sensor.TextSensor), CONF_NAME: f"{config[CONF_NAME]} IAQ Texto"}
    )
    cg.add(var.iaq_numeric, iaq_num)
    cg.add(var.iaq_text, iaq_txt)