import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME
)

CONF_KEY = "key"
CONF_LAST_KEY = "last_key"
AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@eigger"]
MULTI_CONF = True

lilygo_t_keyboard_ns = cg.esphome_ns.namespace("lilygo_t_keyboard")

LilygoTKeyboard = lilygo_t_keyboard_ns.class_("LilygoTKeyboard", cg.Component)
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LilygoTKeyboard),
            cv.Optional(CONF_KEY): text_sensor.text_sensor_schema(
                icon="mdi:chevron-right"
            ),
            cv.Optional(CONF_LAST_KEY): text_sensor.text_sensor_schema(
                icon="mdi:keyboard"
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if CONF_KEY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_KEY])
        cg.add(var.set_key(sens))
    if CONF_LAST_KEY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_KEY])
        cg.add(var.set_last_key(sens))
