import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, uartex
from esphome.const import CONF_OUTPUT_ID, CONF_SPEED, CONF_LOW, CONF_MEDIUM, CONF_HIGH, \
    CONF_STATE, CONF_COMMAND, CONF_UPDATE_INTERVAL
from .. import uartex_ns, state_schema, command_hex_schema, state_hex_expression, \
    command_hex_expression

DEPENDENCIES = ['uartex']
UARTExFan = uartex_ns.class_('UARTExFan', cg.Component)

SPEED_SCHEMA = cv.Schema({
    cv.Optional(CONF_STATE, default={}): state_schema,
    cv.Optional(CONF_COMMAND, default={}): command_hex_schema
})

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(UARTExFan),
    cv.Optional(CONF_SPEED, default={}): cv.Schema({
        cv.Optional(CONF_LOW): SPEED_SCHEMA,
        cv.Optional(CONF_MEDIUM): SPEED_SCHEMA,
        cv.Optional(CONF_HIGH): SPEED_SCHEMA
    }),
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    interval = config[CONF_UPDATE_INTERVAL]
    del config[CONF_UPDATE_INTERVAL]
    config[CONF_UPDATE_INTERVAL] = interval
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)
    speeds = config[CONF_SPEED]
    if CONF_LOW in speeds:
        speed = speeds[CONF_LOW]
        state = yield state_hex_expression(speed[CONF_STATE])
        command = yield command_hex_expression(speed[CONF_COMMAND])
        cg.add(var.set_speed_low(state, command))

    if CONF_MEDIUM in speeds:
        speed = speeds[CONF_MEDIUM]
        state = yield state_hex_expression(speed[CONF_STATE])
        command = yield command_hex_expression(speed[CONF_COMMAND])
        cg.add(var.set_speed_medium(state, command))

    if CONF_HIGH in speeds:
        speed = speeds[CONF_HIGH]
        state = yield state_hex_expression(speed[CONF_STATE])
        command = yield command_hex_expression(speed[CONF_COMMAND])
        cg.add(var.set_speed_high(state, command))

    yield uartex.register_uartex_device(var, config)
