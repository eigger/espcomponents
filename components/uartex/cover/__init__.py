import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, uartex
from esphome.const import CONF_ID
from .. import uartex_ns, UARTExDevice, \
    state_schema, state_num_schema, state_hex_expression, state_num_expression, \
    command_hex_schema, command_expression, command_float_expression
from ..const import CONF_COMMAND_OPEN, CONF_COMMAND_CLOSE, CONF_COMMAND_STOP, CONF_STATE_OPEN, CONF_STATE_CLOSED, CONF_STATE_POSITION, CONF_STATE_TILT, \
    CONF_COMMAND_ON, CONF_COMMAND_OFF, CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_POSITION, CONF_COMMAND_TILT

DEPENDENCIES = ['uartex']
UARTExCover = uartex_ns.class_('UARTExCover', cover.Cover, UARTExDevice)

CONFIG_SCHEMA = cv.All(cover.cover_schema(UARTExCover).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE_OPEN): state_schema,
    cv.Optional(CONF_STATE_CLOSED): state_schema,
    cv.Optional(CONF_STATE_POSITION): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_TILT): cv.templatable(state_num_schema),
    cv.Optional(CONF_COMMAND_OPEN): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_CLOSE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_STOP): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_POSITION): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_TILT): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Cover do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Cover do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Cover do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Cover do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_OPEN in config:
        state = state_hex_expression(config[CONF_STATE_OPEN])
        cg.add(var.set_state(CONF_STATE_OPEN, state))

    if CONF_STATE_CLOSED in config:
        state = state_hex_expression(config[CONF_STATE_CLOSED])
        cg.add(var.set_state(CONF_STATE_CLOSED, state))

    if CONF_STATE_POSITION in config:
        state = await state_num_expression(config[CONF_STATE_POSITION])
        cg.add(var.set_state(CONF_STATE_POSITION, state))

    if CONF_STATE_TILT in config:
        state = await state_num_expression(config[CONF_STATE_TILT])
        cg.add(var.set_state(CONF_STATE_TILT, state))

    if CONF_COMMAND_OPEN in config:
        command = await command_expression(config[CONF_COMMAND_OPEN])
        cg.add(var.set_command(CONF_COMMAND_OPEN, command))

    if CONF_COMMAND_CLOSE in config:
        command = await command_expression(config[CONF_COMMAND_CLOSE])
        cg.add(var.set_command(CONF_COMMAND_CLOSE, command))

    if CONF_COMMAND_STOP in config:
        command = await command_expression(config[CONF_COMMAND_STOP])
        cg.add(var.set_command(CONF_COMMAND_STOP, command))

    if CONF_COMMAND_POSITION in config:
        command = await command_float_expression(config[CONF_COMMAND_POSITION])
        cg.add(var.set_command(CONF_COMMAND_POSITION, command))

    if CONF_COMMAND_TILT in config:
        command = await command_float_expression(config[CONF_COMMAND_TILT])
        cg.add(var.set_command(CONF_COMMAND_TILT, command))