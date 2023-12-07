import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME
)

CONF_PRESS_KEY = "press_key"
CONF_RELEASE_KEY = "release_key"
AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@eigger"]
MULTI_CONF = True

lilygo_t_keyboard_ns = cg.esphome_ns.namespace("lilygo_t_keyboard")

LilygoTKeyboard = lilygo_t_keyboard_ns.class_("LilygoTKeyboard", cg.Component)
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LilygoTKeyboard),
            cv.Optional(CONF_PRESS_KEY): text_sensor.text_sensor_schema(
                icon="mdi:keyboard"
            ),
            cv.Optional(CONF_RELEASE_KEY): text_sensor.text_sensor_schema(
                icon="mdi:keyboard"
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_PRESS_KEY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PRESS_KEY])
        cg.add(var.set_press_key(sens))
    if CONF_RELEASE_KEY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_RELEASE_KEY])
        cg.add(var.set_release_key(sens))
