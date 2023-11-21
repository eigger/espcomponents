import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, number, text_sensor
from esphome.components.text_sensor import register_text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
)

CONF_BRIGHTNESS = "brightness"
CONF_KEY = "key"
CONF_KEY_STATE = "key_state"
AUTO_LOAD = ["text_sensor", "number"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

bbq10_keyboard_ns = cg.esphome_ns.namespace("bbq10_keyboard")

BBQ10Keyboard = bbq10_keyboard_ns.class_("BBQ10Keyboard", cg.Component, i2c.I2CDevice)
Brightness = bbq10_keyboard_ns.class_("Brightness", BBQ10Keyboard)
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(BBQ10Keyboard),
            cv.Optional(CONF_KEY, default={CONF_NAME: "Key"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
            }),
            cv.Optional(CONF_KEY_STATE, default={CONF_NAME: "KeyState"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
            }),
            cv.Optional(CONF_BRIGHTNESS, default={CONF_NAME: "Brightness"}):  number.NUMBER_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(Brightness),
            }),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x1f))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if CONF_KEY in config:
        sens = cg.new_Pvariable(config[CONF_KEY][CONF_ID])
        await register_text_sensor(sens, config[CONF_KEY])
        cg.add(var.set_key(sens))
    if CONF_KEY_STATE in config:
        sens = cg.new_Pvariable(config[CONF_KEY_STATE][CONF_ID])
        await register_text_sensor(sens, config[CONF_KEY_STATE])
        cg.add(var.set_key_state(sens))
    if CONF_BRIGHTNESS in config:
        sens = cg.new_Pvariable(config[CONF_BRIGHTNESS][CONF_ID])
        await number.register_number(sens, config[CONF_BRIGHTNESS],
            min_value = 0,
            max_value = 100,
            step = 0x01)
        cg.add(var.set_brightness(sens))

