import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import uartex_ns, UARTExDevice, \
    uartex_declare_type, \
    state_schema, state_string_expression, \
    command_hex_schema, command_string_expression
from ..const import CONF_UARTEX_ID, CONF_COMMAND_TEXT, CONF_COMMAND_UPDATE, CONF_STATE

DEPENDENCIES = ['uartex']
UARTExText = uartex_ns.class_('UARTExText', text.Text, UARTExDevice)

CONFIG_SCHEMA = cv.All(text.TEXT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExText),
    cv.GenerateID(CONF_UARTEX_ID): uartex_declare_type,
    cv.Required(CONF_COMMAND_TEXT): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_UPDATE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_STATE): state_schema,
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s')))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text.register_text(var, config)
    await uartex.register_uartex_device(var, config)
    
    command = await command_string_expression(config[CONF_COMMAND_TEXT])
    cg.add(var.set_command(CONF_COMMAND_TEXT, command))
    
    if CONF_LAMBDA in config:
        state = await state_string_expression(config[CONF_LAMBDA])
        cg.add(var.set_state(CONF_LAMBDA, state))
    
