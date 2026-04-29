"""CM1106SL-NS Sensor component for ESPHome."""

import esphome.codegen as cg
from esphome.components import sensor, binary_sensor, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_CO2,
    CONF_ID,
    DEVICE_CLASS_CARBON_DIOXIDE,
    ICON_MOLECULE_CO2,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["binary_sensor"]
CODEOWNERS = ["@rafaalbelda"]

cm1106sl_ns_ns = cg.esphome_ns.namespace("cm1106sl_ns")
CM1106SLNSComponent = cm1106sl_ns_ns.class_(
    "CM1106SLNSComponent", cg.Component, i2c.I2CDevice
)

CONF_STABILITY = "stability"
CONF_READY = "ready"
CONF_ERROR = "error"
CONF_IAQ_NUMERIC = "iaq_numeric"
CONF_STATUS = "status"
CONF_DEBUG = "debug"
CONF_MEASUREMENT_PERIOD = "measurement_period"

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
            cv.Optional(CONF_STATUS): sensor.sensor_schema(
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.GenerateID(): cv.declare_id(sensor.Sensor),
                }
            ),
            cv.Optional(CONF_DEBUG, default=False): cv.boolean,
            cv.Optional(CONF_MEASUREMENT_PERIOD, default="60s"): cv.positive_time_period_milliseconds,
        },
    )
    .extend(i2c.i2c_device_schema(0x31))
)


async def to_code(config) -> None:
    """Code generation entry point."""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if co2_config := config.get(CONF_CO2):
        sens = await sensor.new_sensor(co2_config)
        cg.add(var.set_co2_sensor(sens))

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

    if status_config := config.get(CONF_STATUS):
        sens = await sensor.new_sensor(status_config)
        cg.add(var.set_status_sensor(sens))

    # Debug flag for I2C logging
    cg.add(var.set_debug(config[CONF_DEBUG]))

    # Read interval for the command I2C protocol
    cg.add(var.set_measurement_period(config[CONF_MEASUREMENT_PERIOD]))
