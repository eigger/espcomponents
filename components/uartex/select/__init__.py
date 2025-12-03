import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select, uartex
from esphome.const import CONF_ID, CONF_INITIAL_OPTION, CONF_OPTIONS, CONF_RESTORE_VALUE
from .. import uartex_ns, UARTExDevice, \
    state_string_expression, \
    command_hex_schema, command_string_expression
from ..const import CONF_COMMAND_SELECT, CONF_COMMAND_ON, CONF_COMMAND_OFF, CONF_STATE_ON, CONF_STATE_OFF, CONF_STATE_SELECT

DEPENDENCIES = ['uartex']
UARTExSelect = uartex_ns.class_('UARTExSelect', select.Select, UARTExDevice)

CONFIG_SCHEMA = cv.All(select.select_schema(UARTExSelect).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Required(CONF_OPTIONS): cv.All(
        cv.ensure_list(cv.string_strict), cv.Length(min=1)
    ),
    cv.Optional(CONF_INITIAL_OPTION): cv.string_strict,
    cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
    cv.Optional(CONF_COMMAND_SELECT): cv.templatable(command_hex_schema),
    cv.Optional(CONF_STATE_SELECT): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Text do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Text do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Text do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Text do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = await select.new_select(config, options=config[CONF_OPTIONS])
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_COMMAND_SELECT in config:
        command = await command_string_expression(config[CONF_COMMAND_SELECT])
        cg.add(var.set_command(CONF_COMMAND_SELECT, command))
    
    if CONF_STATE_SELECT in config:
        state = await state_string_expression(config[CONF_STATE_SELECT])
        cg.add(var.set_state(CONF_STATE_SELECT, state))
    
    if CONF_INITIAL_OPTION in config:
        initial_option_index = config[CONF_OPTIONS].index(config[CONF_INITIAL_OPTION])
        if initial_option_index != 0:
            cg.add(var.set_initial_option_index(initial_option_index))

    if CONF_RESTORE_VALUE in config:
        cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))