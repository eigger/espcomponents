import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import uartex_ns, UARTExDevice, \
    state_string_expression, \
    command_hex_schema, command_string_expression
from ..const import CONF_COMMAND_TEXT, CONF_COMMAND_ON, CONF_COMMAND_OFF, CONF_STATE_ON, CONF_STATE_OFF

DEPENDENCIES = ['uartex']
UARTExText = uartex_ns.class_('UARTExText', text.Text, UARTExDevice)

CONFIG_SCHEMA = cv.All(text.text_schema(UARTExText).extend({
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Required(CONF_COMMAND_TEXT): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Text do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Text do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Text do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Text do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = await text.new_text(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_COMMAND_TEXT in config:
        command = await command_string_expression(config[CONF_COMMAND_TEXT])
        cg.add(var.set_command(CONF_COMMAND_TEXT, command))
    
    if CONF_LAMBDA in config:
        state = await state_string_expression(config[CONF_LAMBDA])
        cg.add(var.set_state(CONF_LAMBDA, state))
    
