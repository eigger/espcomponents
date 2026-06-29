import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from . import M5UnitScalesComponent, CONF_M5UNIT_SCALES_ID, m5unit_scales_ns

DEPENDENCIES = ["m5unit_scales"]

CONF_OUTPUT_ID = "output_id"

M5UnitScalesLED = m5unit_scales_ns.class_(
    "M5UnitScalesLED", light.LightOutput
)

CONFIG_SCHEMA = light.light_schema(
    M5UnitScalesLED,
    light.LightType.RGB,
).extend({
    cv.GenerateID(CONF_M5UNIT_SCALES_ID): cv.use_id(M5UnitScalesComponent),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5UNIT_SCALES_ID])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    cg.add(var.set_parent(parent))
    cg.add(parent.set_led_light(var))
