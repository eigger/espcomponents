import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, rs485
from esphome.const import CONF_OUTPUT_ID, CONF_SPEED, CONF_LOW, CONF_MEDIUM, CONF_HIGH, \
                          CONF_STATE, CONF_COMMAND, CONF_UPDATE_INTERVAL
from .. import rs485_ns, state_hex_schema, command_hex_schema, state_hex_expression, \
               command_hex_expression

DEPENDENCIES = ['rs485']
RS485Fan = rs485_ns.class_('RS485Fan', cg.Component)

SPEED_SCHEMA = cv.Schema({
    cv.Optional(CONF_STATE, default={}): state_hex_schema,
    cv.Optional(CONF_COMMAND, default={}): command_hex_schema
})

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RS485Fan),
    cv.Optional(CONF_SPEED, default={}): cv.Schema({
        cv.Optional(CONF_LOW): SPEED_SCHEMA,
        cv.Optional(CONF_MEDIUM): SPEED_SCHEMA,
        cv.Optional(CONF_HIGH): SPEED_SCHEMA
    }),
}).extend(rs485.RS485_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    interval = config[CONF_UPDATE_INTERVAL]
    del config[CONF_UPDATE_INTERVAL]
    fan_state = yield fan.create_fan_state(config)

    config[CONF_UPDATE_INTERVAL] = interval
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID], fan_state)
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

    yield rs485.register_rs485_device(var, config)
