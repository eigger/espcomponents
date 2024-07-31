import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import valve, uartex
from esphome import core
from esphome.const import CONF_ID
from .. import uartex_ns, UARTExComponent, uint8_ptr_const, uint16_const, \
    command_hex_schema, command_hex_expression, state_schema, state_hex_expression
from ..const import CONF_COMMAND_OPEN, CONF_COMMAND_CLOSE, CONF_COMMAND_STOP, CONF_STATE_OPEN, CONF_STATE_CLOSED, CONF_STATE_POSITION, \
    CONF_COMMAND_ON, CONF_COMMAND_OFF, CONF_STATE_ON, CONF_STATE_OFF

DEPENDENCIES = ['uartex']
UARTExValve = uartex_ns.class_('UARTExValve', valve.Valve, UARTExComponent)

CONFIG_SCHEMA = cv.All(valve.VALVE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExValve),
    cv.Optional(CONF_STATE_OPEN): state_schema,
    cv.Optional(CONF_STATE_CLOSED): state_schema,
    cv.Optional(CONF_STATE_POSITION): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_OPEN): command_hex_schema,
    cv.Optional(CONF_COMMAND_CLOSE): command_hex_schema,
    cv.Optional(CONF_COMMAND_STOP): command_hex_schema,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Valve do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Valve do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Valve do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Valve do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await valve.register_valve(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_OPEN in config:
        args = state_hex_expression(config[CONF_STATE_OPEN])
        cg.add(var.set_state(CONF_STATE_OPEN, args))
    if CONF_STATE_CLOSED in config:
        args = state_hex_expression(config[CONF_STATE_CLOSED])
        cg.add(var.set_state(CONF_STATE_CLOSED, args))
    if CONF_STATE_POSITION in config:
        args = await cg.templatable(config[CONF_STATE_POSITION], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
        cg.add(var.set_state(CONF_STATE_POSITION, args))
    if CONF_COMMAND_OPEN in config:
        args = command_hex_expression(config[CONF_COMMAND_OPEN])
        cg.add(var.set_command(CONF_COMMAND_OPEN, args))
    if CONF_COMMAND_CLOSE in config:
        args = command_hex_expression(config[CONF_COMMAND_CLOSE])
        cg.add(var.set_command(CONF_COMMAND_CLOSE, args))
    if CONF_COMMAND_STOP in config:
        args = command_hex_expression(config[CONF_COMMAND_STOP])
        cg.add(var.set_command(CONF_COMMAND_STOP, args))