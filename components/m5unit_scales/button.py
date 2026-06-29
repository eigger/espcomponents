import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from . import M5UnitScalesComponent, CONF_M5UNIT_SCALES_ID, m5unit_scales_ns

DEPENDENCIES = ["m5unit_scales"]

CONF_TARE = "tare"

M5UnitScalesTareButton = m5unit_scales_ns.class_(
    "M5UnitScalesTareButton", button.Button
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_M5UNIT_SCALES_ID): cv.use_id(M5UnitScalesComponent),
    cv.Optional(CONF_TARE): button.button_schema(M5UnitScalesTareButton),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5UNIT_SCALES_ID])
    if CONF_TARE in config:
        var = await button.new_button(config[CONF_TARE])
        cg.add(var.set_parent(parent))
        cg.add(parent.set_tare_button(var))
