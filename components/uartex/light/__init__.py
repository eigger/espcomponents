import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, uartex
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT_ID, CONF_UPDATE_INTERVAL
from ..const import CONF_COMMAND_BRIGHTNESS, CONF_STATE_BRIGHTNESS
from .. import uartex_ns, cmd_t, uint8_ptr_const, uint16_const

DEPENDENCIES = ['uartex']
UARTExLightOutput = uartex_ns.class_('UARTExLightOutput', light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(UARTExLightOutput),
    cv.Optional(CONF_STATE_BRIGHTNESS): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_BRIGHTNESS): cv.returning_lambda,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    del config[CONF_UPDATE_INTERVAL]
    await light.register_light(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_BRIGHTNESS in config:
        templ = await cg.templatable(config[CONF_STATE_BRIGHTNESS], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
        cg.add(var.set_state_brightness(templ))
    if CONF_COMMAND_BRIGHTNESS in config:
        templ = await cg.templatable(config[CONF_COMMAND_BRIGHTNESS], [(cg.float_.operator('const'), 'x')], cmd_t)
        cg.add(var.set_command_brightness(templ))

