import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, number, text_sensor
from esphome.components.text_sensor import register_text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
)

CONF_KEY = "key"
AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

bbq10_keyboard_ns = cg.esphome_ns.namespace("bbq10_keyboard")

BBQ10Keyboard = bbq10_keyboard_ns.class_("BBQ10Keyboard", cg.Component, i2c.I2CDevice)
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BBQ10Keyboard),
            # cv.Optional(CONF_KEY, default={CONF_NAME: "Key"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            # {
            #     cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
            # }),
            cv.Optional(CONF_KEY, default="Key"): text_sensor.TEXT_SENSOR_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x55))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    # if CONF_KEY in config:
    #     sens = cg.new_Pvariable(config[CONF_KEY][CONF_ID])
    #     await register_text_sensor(sens, config[CONF_KEY])
    #     cg.add(var.set_key(sens))
    if CONF_KEY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_KEY])
        cg.add(var.set_key(sens))

