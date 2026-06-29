import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from . import M5UnitScalesComponent, CONF_M5UNIT_SCALES_ID

DEPENDENCIES = ["m5unit_scales"]

CONF_BUTTON = "button"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_M5UNIT_SCALES_ID): cv.use_id(M5UnitScalesComponent),
    cv.Optional(CONF_BUTTON): binary_sensor.binary_sensor_schema(),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5UNIT_SCALES_ID])
    if CONF_BUTTON in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_BUTTON])
        cg.add(parent.set_button_sensor(sens))
