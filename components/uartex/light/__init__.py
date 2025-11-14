import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, uartex
from esphome.const import CONF_OUTPUT_ID, CONF_UPDATE_INTERVAL
from .. import uartex_ns, UARTExDevice, \
    state_num_schema, state_num_expression, \
    command_hex_schema, command_float_expression, validate_version
from ..const import CONF_COMMAND_BRIGHTNESS, CONF_STATE_BRIGHTNESS

DEPENDENCIES = ['uartex']
UARTExLightOutput = uartex_ns.class_('UARTExLightOutput', light.LightOutput, UARTExDevice)
UARTExLightState = uartex_ns.class_('UARTExLightState', light.LightState)

CONFIG_SCHEMA = cv.All(light.light_schema(UARTExLightOutput, light.LightType.BINARY).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.GenerateID(): cv.declare_id(UARTExLightState),
    cv.Optional(CONF_STATE_BRIGHTNESS): cv.templatable(state_num_schema),
    cv.Optional(CONF_COMMAND_BRIGHTNESS): cv.templatable(command_hex_schema),
}).extend(cv.COMPONENT_SCHEMA), validate_version)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    del config[CONF_UPDATE_INTERVAL]
    await light.register_light(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_STATE_BRIGHTNESS in config:
        state = await state_num_expression(config[CONF_STATE_BRIGHTNESS])
        cg.add(var.set_state(CONF_STATE_BRIGHTNESS, state))
        
    if CONF_COMMAND_BRIGHTNESS in config:
        command = await command_float_expression(config[CONF_COMMAND_BRIGHTNESS])
        cg.add(var.set_command(CONF_COMMAND_BRIGHTNESS, command))

