import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, uartex
from esphome.const import CONF_ID, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP, CONF_RESTORE_VALUE
from .. import uartex_ns, UARTExDevice, \
    state_schema, state_hex_expression, state_num_schema, state_num_expression, \
    command_hex_schema, command_float_expression
from ..const import CONF_COMMAND_NUMBER, CONF_COMMAND_OFF, CONF_COMMAND_ON, \
    CONF_STATE_NUMBER, CONF_STATE_OFF, CONF_STATE_ON, CONF_STATE_INCREMENT, CONF_STATE_DECREMENT, CONF_STATE_TO_MIN, CONF_STATE_TO_MAX

DEPENDENCIES = ['uartex']
UARTExNumber = uartex_ns.class_('UARTExNumber', number.Number, UARTExDevice)

CONFIG_SCHEMA = cv.All(number.number_schema(UARTExNumber).extend({
    cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
    cv.Optional(CONF_MAX_VALUE, default=10): cv.float_,
    cv.Optional(CONF_STEP, default=1): cv.float_,
    cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE_NUMBER): cv.templatable(state_num_schema),
    cv.Optional(CONF_COMMAND_NUMBER): cv.templatable(command_hex_schema),
    cv.Optional(CONF_STATE_INCREMENT): state_schema,
    cv.Optional(CONF_STATE_DECREMENT): state_schema,
    cv.Optional(CONF_STATE_TO_MIN): state_schema,
    cv.Optional(CONF_STATE_TO_MAX): state_schema,
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Number do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Number do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Number do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Number do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_RESTORE_VALUE in config:
        cg.add(var.set_restore_value(config[CONF_RESTORE_VALUE]))

    if CONF_STATE_NUMBER in config:
        state = await state_num_expression(config[CONF_STATE_NUMBER])
        cg.add(var.set_state(CONF_STATE_NUMBER, state))

    if CONF_STATE_INCREMENT in config:
        state = state_hex_expression(config[CONF_STATE_INCREMENT])
        cg.add(var.set_state(CONF_STATE_INCREMENT, state))

    if CONF_STATE_DECREMENT in config:
        state = state_hex_expression(config[CONF_STATE_DECREMENT])
        cg.add(var.set_state(CONF_STATE_DECREMENT, state))

    if CONF_STATE_TO_MIN in config:
        state = state_hex_expression(config[CONF_STATE_TO_MIN])
        cg.add(var.set_state(CONF_STATE_TO_MIN, state))

    if CONF_STATE_TO_MAX in config:
        state = state_hex_expression(config[CONF_STATE_TO_MAX])
        cg.add(var.set_state(CONF_STATE_TO_MAX, state))

    if CONF_COMMAND_NUMBER in config:
        command = await command_float_expression(config[CONF_COMMAND_NUMBER])
        cg.add(var.set_command(CONF_COMMAND_NUMBER, command))
        
