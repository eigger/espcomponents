import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, wallpad
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT_ID, CONF_DEVICE, CONF_UPDATE_INTERVAL
from .. import wallpad_ns

DEPENDENCIES = ['wallpad']
WallPadLightOutput = wallpad_ns.class_(
    'WallPadLightOutput', light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(WallPadLightOutput),
}).extend(wallpad.WallPad_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    light_var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], var)
    cg.add(var.set_light(light_var))

    cg.add(cg.App.register_light(light_var))
    yield cg.register_component(var, config)
    del config[CONF_UPDATE_INTERVAL]
    yield cg.register_component(light_var, config)
    yield light.setup_light_core_(light_var, var, config)

    yield wallpad.register_wallpad_device(var, config)
