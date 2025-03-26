import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, uartex
from esphome.const import CONF_ID, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP
from .. import uartex_ns, \
    state_num_schema, state_num_expression, \
    command_hex_schema, command_float_expression
from ..const import CONF_COMMAND_NUMBER, CONF_COMMAND_OFF, CONF_STATE_NUMBER, CONF_STATE_OFF, \
    CONF_COMMAND_ON, CONF_STATE_ON

DEPENDENCIES = ['uartex']
UARTExNumber = uartex_ns.class_('UARTExNumber', number.Number, cg.Component)

CONFIG_SCHEMA = cv.All(number.NUMBER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExNumber),
    cv.Required(CONF_MIN_VALUE): cv.float_,
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Required(CONF_STEP): cv.float_,
    cv.Optional(CONF_STATE_NUMBER): cv.templatable(state_num_schema),
    cv.Optional(CONF_COMMAND_NUMBER): cv.templatable(command_hex_schema),
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Number do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Number do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Number do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Number do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_STATE_NUMBER, CONF_COMMAND_NUMBER))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(            
        var,
        config,
        min_value = config[CONF_MIN_VALUE],
        max_value = config[CONF_MAX_VALUE],
        step = config[CONF_STEP],)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_NUMBER in config:
        state = await state_num_expression(config[CONF_STATE_NUMBER])
        cg.add(var.set_state(CONF_STATE_NUMBER, state))

    if CONF_COMMAND_NUMBER in config:
        command = await command_float_expression(config[CONF_COMMAND_NUMBER])
        cg.add(var.set_command(CONF_COMMAND_NUMBER, command))
        
