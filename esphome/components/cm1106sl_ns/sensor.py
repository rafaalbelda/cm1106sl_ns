"""CM1106SL-NS Sensor component for ESPHome."""

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import sensor, text_sensor, binary_sensor, uart
import esphome.config_validation as cv
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
CODEOWNERS = ["@rafaalbelda"]

cm1106sl_ns_ns = cg.esphome_ns.namespace("cm1106sl_ns")
CM1106SLNSComponent = cm1106sl_ns_ns.class_(
    "CM1106SLNSComponent", cg.PollingComponent, uart.UARTDevice
)

CONF_DF3 = "df3"
CONF_DF4 = "df4"
CONF_STATUS = "status"
CONF_STABILITY = "stability"
CONF_READY = "ready"
CONF_ERROR = "error"
CONF_IAQ_NUMERIC = "iaq_numeric"
CONF_IAQ_TEXT = "iaq_text"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CM1106SLNSComponent),
            cv.Optional(CONF_CO2): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                icon=ICON_MOLECULE_CO2,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_CARBON_DIOXIDE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.GenerateID(): cv.declare_id(sensor.Sensor),
                }
            ),
            cv.Optional(CONF_DF3): sensor.sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(sensor.Sensor),
                }
            ),
            cv.Optional(CONF_DF4): sensor.sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(sensor.Sensor),
                }
            ),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
                }
            ),
            cv.Optional(CONF_STABILITY): sensor.sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(sensor.Sensor),
                }
            ),
            cv.Optional(CONF_READY): binary_sensor.binary_sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(binary_sensor.BinarySensor),
                }
            ),
            cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(binary_sensor.BinarySensor),
                }
            ),
            cv.Optional(CONF_IAQ_NUMERIC): sensor.sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(sensor.Sensor),
                }
            ),
            cv.Optional(CONF_IAQ_TEXT): text_sensor.text_sensor_schema().extend(
                {
                    cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
                }
            ),
        },
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config) -> None:
    """Code generation entry point."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if co2_config := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2_config)
        cg.add(var.set_co2_sensor(sens))

    if df3_config := config.get(CONF_DF3):
        sens = await sensor.new_sensor(df3_config)
        cg.add(var.set_df3_sensor(sens))

    if df4_config := config.get(CONF_DF4):
        sens = await sensor.new_sensor(df4_config)
        cg.add(var.set_df4_sensor(sens))

    if status_config := config.get(CONF_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(var.set_status_sensor(sens))

    if stability_config := config.get(CONF_STABILITY):
        sens = await sensor.new_sensor(stability_config)
        cg.add(var.set_stability_sensor(sens))

    if ready_config := config.get(CONF_READY):
        sens = await binary_sensor.new_binary_sensor(ready_config)
        cg.add(var.set_ready_sensor(sens))

    if error_config := config.get(CONF_ERROR):
        sens = await binary_sensor.new_binary_sensor(error_config)
        cg.add(var.set_error_sensor(sens))

    if iaq_numeric_config := config.get(CONF_IAQ_NUMERIC):
        sens = await sensor.new_sensor(iaq_numeric_config)
        cg.add(var.set_iaq_numeric_sensor(sens))

    if iaq_text_config := config.get(CONF_IAQ_TEXT):
        sens = await text_sensor.new_text_sensor(iaq_text_config)
        cg.add(var.set_iaq_text_sensor(sens))
