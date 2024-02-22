import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uartex, sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_OFFSET
from .. import uartex_ns, command_hex_schema, STATE_NUM_SCHEMA, cmd_t, uint8_ptr_const, uint16_const, uint8_const, \
    command_hex_expression, state_schema, state_hex_expression
from ..const import CONF_STATE_TEMPERATURE_CURRENT, CONF_STATE_TEMPERATURE_TARGET, \
    CONF_STATE_AUTO, CONF_STATE_HEAT, CONF_STATE_COOL, CONF_STATE_AWAY, \
    CONF_COMMAND_AUTO, CONF_COMMAND_HEAT, CONF_COMMAND_COOL, CONF_COMMAND_AWAY, \
    CONF_COMMAND_TEMPERATURE, CONF_LENGTH, CONF_PRECISION, \
    CONF_COMMAND_ON, CONF_STATE_ON, CONF_COMMAND_HOME


AUTO_LOAD = ['sensor']
DEPENDENCIES = ['uartex']

UARTExClimate = uartex_ns.class_('UARTExClimate', climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExClimate),
    cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_STATE_TEMPERATURE_CURRENT): cv.templatable(STATE_NUM_SCHEMA),
    cv.Required(CONF_STATE_TEMPERATURE_TARGET): cv.templatable(STATE_NUM_SCHEMA),
    cv.Optional(CONF_STATE_AUTO): state_schema,
    cv.Optional(CONF_STATE_HEAT): state_schema,
    cv.Optional(CONF_STATE_COOL): state_schema,
    cv.Optional(CONF_STATE_AWAY): state_schema,
    cv.Required(CONF_COMMAND_TEMPERATURE): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_AUTO): command_hex_schema,
    cv.Optional(CONF_COMMAND_HEAT): command_hex_schema,
    cv.Optional(CONF_COMMAND_COOL): command_hex_schema,
    cv.Optional(CONF_COMMAND_AWAY): command_hex_schema,
    cv.Optional(CONF_COMMAND_HOME): command_hex_schema,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Climate do not support command_on!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Climate do not support state_on!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_exactly_one_key(CONF_SENSOR, CONF_STATE_TEMPERATURE_CURRENT), cv.has_at_least_one_key(CONF_COMMAND_HEAT, CONF_COMMAND_COOL, CONF_COMMAND_AUTO))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield climate.register_climate(var, config)
    yield uartex.register_uartex_device(var, config)

    templ = yield cg.templatable(config[CONF_COMMAND_TEMPERATURE], [(cg.float_.operator('const'), 'x'), (uint8_const, 'mode'), (uint8_const, 'preset')], cmd_t)
    cg.add(var.set_command_temperature(templ))

    state = config[CONF_STATE_TEMPERATURE_TARGET]
    if cg.is_template(state):
        templ = yield cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
        cg.add(var.set_state_target(templ))
    else:
        args = yield state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
        cg.add(var.set_state_target(args))

    if CONF_SENSOR in config:
        sens = yield cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_STATE_TEMPERATURE_CURRENT in config:
        state = config[CONF_STATE_TEMPERATURE_CURRENT]
        if cg.is_template(state):
            templ = yield cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state_current(templ))
        else:
            args = yield state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state_current(args))

    if CONF_STATE_AUTO in config:
        args = yield state_hex_expression(config[CONF_STATE_AUTO])
        cg.add(var.set_state_auto(args))

    if CONF_STATE_HEAT in config:
        args = yield state_hex_expression(config[CONF_STATE_HEAT])
        cg.add(var.set_state_heat(args))

    if CONF_STATE_COOL in config:
        args = yield state_hex_expression(config[CONF_STATE_COOL])
        cg.add(var.set_state_cool(args))

    if CONF_STATE_AWAY in config:
        args = yield state_hex_expression(config[CONF_STATE_AWAY])
        cg.add(var.set_state_away(args))

    if CONF_COMMAND_AUTO in config:
        args = yield command_hex_expression(config[CONF_COMMAND_AUTO])
        cg.add(var.set_command_auto(args))

    if CONF_COMMAND_HEAT in config:
        args = yield command_hex_expression(config[CONF_COMMAND_HEAT])
        cg.add(var.set_command_heat(args))

    if CONF_COMMAND_COOL in config:
        args = yield command_hex_expression(config[CONF_COMMAND_COOL])
        cg.add(var.set_command_cool(args))

    if CONF_COMMAND_AWAY in config:
        args = yield command_hex_expression(config[CONF_COMMAND_AWAY])
        cg.add(var.set_command_away(args))
