import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, uartex
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT_ID, CONF_UPDATE_INTERVAL
from .. import uartex_ns

DEPENDENCIES = ['uartex']
UARTExLightOutput = uartex_ns.class_(
    'UARTExLightOutput', light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(UARTExLightOutput),
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)
    del config[CONF_UPDATE_INTERVAL]
    yield light.register_light(var, config)
    yield uartex.register_uartex_device(var, config)
