import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from . import M5UnitScalesComponent, CONF_M5UNIT_SCALES_ID, m5unit_scales_ns

DEPENDENCIES = ["m5unit_scales"]

CONF_LP_FILTER = "lp_filter"

M5UnitScalesLPFilterSwitch = m5unit_scales_ns.class_(
    "M5UnitScalesLPFilterSwitch", switch.Switch
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_M5UNIT_SCALES_ID): cv.use_id(M5UnitScalesComponent),
    cv.Optional(CONF_LP_FILTER): switch.switch_schema(M5UnitScalesLPFilterSwitch),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5UNIT_SCALES_ID])
    if CONF_LP_FILTER in config:
        var = await switch.new_switch(config[CONF_LP_FILTER])
        cg.add(var.set_parent(parent))
        cg.add(parent.set_lp_filter_switch(var))
