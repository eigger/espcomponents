import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uartex, sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_OFFSET
from .. import uartex_ns, command_hex_schema, STATE_NUM_SCHEMA, cmd_t, uint8_ptr_const, uint16_const, uint8_const, \
    await command_hex_expression, state_schema, await state_hex_expression
from ..const import CONF_STATE_TEMPERATURE_CURRENT, CONF_STATE_TEMPERATURE_TARGET, \
    CONF_STATE_ON, CONF_STATE_AUTO, CONF_STATE_HEAT, CONF_STATE_COOL, CONF_STATE_FAN_ONLY, CONF_STATE_DRY, CONF_STATE_SWING_OFF, CONF_STATE_SWING_BOTH, CONF_STATE_SWING_VERTICAL, CONF_STATE_SWING_HORIZONTAL, \
    CONF_COMMAND_ON, CONF_COMMAND_AUTO, CONF_COMMAND_HEAT, CONF_COMMAND_COOL, CONF_COMMAND_FAN_ONLY, CONF_COMMAND_DRY, CONF_COMMAND_SWING_OFF, CONF_COMMAND_SWING_BOTH, CONF_COMMAND_SWING_VERTICAL, CONF_COMMAND_SWING_HORIZONTAL, \
    CONF_COMMAND_TEMPERATURE, CONF_LENGTH, CONF_PRECISION, CONF_COMMAND_PRESET_ECO, CONF_COMMAND_PRESET_SLEEP, CONF_COMMAND_PRESET_ACTIVITY, \
    CONF_COMMAND_ON, CONF_COMMAND_PRESET_NONE, CONF_COMMAND_PRESET_HOME, CONF_COMMAND_PRESET_AWAY, CONF_COMMAND_PRESET_BOOST, CONF_COMMAND_PRESET_COMFORT, \
    CONF_STATE_PRESET_NONE, CONF_STATE_PRESET_HOME, CONF_STATE_PRESET_AWAY, CONF_STATE_PRESET_BOOST, CONF_STATE_PRESET_COMFORT, CONF_STATE_PRESET_ECO, CONF_STATE_PRESET_SLEEP, CONF_STATE_PRESET_ACTIVITY
    

AUTO_LOAD = ['sensor']
DEPENDENCIES = ['uartex']

UARTExClimate = uartex_ns.class_('UARTExClimate', climate.Climate, cg.Component)

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExClimate),
    cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_STATE_TEMPERATURE_CURRENT): cv.templatable(STATE_NUM_SCHEMA),
    cv.Required(CONF_STATE_TEMPERATURE_TARGET): cv.templatable(STATE_NUM_SCHEMA),
    cv.Optional(CONF_STATE_COOL): state_schema,
    cv.Optional(CONF_STATE_HEAT): state_schema,
    cv.Optional(CONF_STATE_FAN_ONLY): state_schema,
    cv.Optional(CONF_STATE_DRY): state_schema,
    cv.Optional(CONF_STATE_AUTO): state_schema,
    cv.Optional(CONF_STATE_SWING_OFF): state_schema,
    cv.Optional(CONF_STATE_SWING_BOTH): state_schema,
    cv.Optional(CONF_STATE_SWING_VERTICAL): state_schema,
    cv.Optional(CONF_STATE_SWING_HORIZONTAL): state_schema,
    cv.Optional(CONF_STATE_PRESET_NONE): state_schema,
    cv.Optional(CONF_STATE_PRESET_HOME): state_schema,
    cv.Optional(CONF_STATE_PRESET_AWAY): state_schema,
    cv.Optional(CONF_STATE_PRESET_BOOST): state_schema,
    cv.Optional(CONF_STATE_PRESET_COMFORT): state_schema,
    cv.Optional(CONF_STATE_PRESET_ECO): state_schema,
    cv.Optional(CONF_STATE_PRESET_SLEEP): state_schema,
    cv.Optional(CONF_STATE_PRESET_ACTIVITY): state_schema,
    cv.Required(CONF_COMMAND_TEMPERATURE): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_COOL): command_hex_schema,
    cv.Optional(CONF_COMMAND_HEAT): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_ONLY): command_hex_schema,
    cv.Optional(CONF_COMMAND_DRY): command_hex_schema,
    cv.Optional(CONF_COMMAND_AUTO): command_hex_schema,
    cv.Optional(CONF_COMMAND_SWING_OFF): command_hex_schema,
    cv.Optional(CONF_COMMAND_SWING_BOTH): command_hex_schema,
    cv.Optional(CONF_COMMAND_SWING_VERTICAL): command_hex_schema,
    cv.Optional(CONF_COMMAND_SWING_HORIZONTAL): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_NONE): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_HOME): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_AWAY): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_BOOST): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_COMFORT): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_ECO): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_SLEEP): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_ACTIVITY): command_hex_schema,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Climate do not support command_on!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Climate do not support state_on!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_exactly_one_key(CONF_SENSOR, CONF_STATE_TEMPERATURE_CURRENT), cv.has_at_least_one_key(CONF_COMMAND_HEAT, CONF_COMMAND_COOL, CONF_COMMAND_AUTO))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uartex.register_uartex_device(var, config)
    templ = await cg.templatable(config[CONF_COMMAND_TEMPERATURE], [(cg.float_.operator('const'), 'x'), (uint8_const, 'mode'), (uint8_const, 'preset')], cmd_t)
    cg.add(var.set_command_temperature(templ))
    state = config[CONF_STATE_TEMPERATURE_TARGET]
    if cg.is_template(state):
        templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
        cg.add(var.set_state_target(templ))
    else:
        args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
        cg.add(var.set_state_target(args))
    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))
    if CONF_STATE_TEMPERATURE_CURRENT in config:
        state = config[CONF_STATE_TEMPERATURE_CURRENT]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state_current(templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state_current(args))
    if CONF_STATE_COOL in config:
        args = await state_hex_expression(config[CONF_STATE_COOL])
        cg.add(var.set_state_cool(args))
    if CONF_STATE_HEAT in config:
        args = await state_hex_expression(config[CONF_STATE_HEAT])
        cg.add(var.set_state_heat(args))
    if CONF_STATE_FAN_ONLY in config:
        args = await state_hex_expression(config[CONF_STATE_FAN_ONLY])
        cg.add(var.set_state_fan_only(args))
    if CONF_STATE_DRY in config:
        args = await state_hex_expression(config[CONF_STATE_DRY])
        cg.add(var.set_state_dry(args))
    if CONF_STATE_AUTO in config:
        args = await state_hex_expression(config[CONF_STATE_AUTO])
        cg.add(var.set_state_auto(args))
    if CONF_STATE_SWING_OFF in config:
        args = await state_hex_expression(config[CONF_STATE_SWING_OFF])
        cg.add(var.set_state_swing_off(args))
    if CONF_STATE_SWING_BOTH in config:
        args = await state_hex_expression(config[CONF_STATE_SWING_BOTH])
        cg.add(var.set_state_swing_both(args))
    if CONF_STATE_SWING_VERTICAL in config:
        args = await state_hex_expression(config[CONF_STATE_SWING_VERTICAL])
        cg.add(var.set_state_swing_vertical(args))
    if CONF_STATE_SWING_HORIZONTAL in config:
        args = await state_hex_expression(config[CONF_STATE_SWING_HORIZONTAL])
        cg.add(var.set_state_swing_horizontal(args))
    if CONF_STATE_PRESET_NONE in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_NONE])
        cg.add(var.set_state_preset_none(args))
    if CONF_STATE_PRESET_HOME in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_HOME])
        cg.add(var.set_state_preset_home(args))
    if CONF_STATE_PRESET_AWAY in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_AWAY])
        cg.add(var.set_state_preset_away(args))
    if CONF_STATE_PRESET_BOOST in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_BOOST])
        cg.add(var.set_state_preset_boost(args))
    if CONF_STATE_PRESET_COMFORT in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_COMFORT])
        cg.add(var.set_state_preset_comfort(args))
    if CONF_STATE_PRESET_ECO in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_ECO])
        cg.add(var.set_state_preset_eco(args))
    if CONF_STATE_PRESET_SLEEP in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_SLEEP])
        cg.add(var.set_state_preset_sleep(args))
    if CONF_STATE_PRESET_ACTIVITY in config:
        args = await state_hex_expression(config[CONF_STATE_PRESET_ACTIVITY])
        cg.add(var.set_state_preset_activity(args))
    if CONF_COMMAND_COOL in config:
        args = await command_hex_expression(config[CONF_COMMAND_COOL])
        cg.add(var.set_command_cool(args))
    if CONF_COMMAND_HEAT in config:
        args = await command_hex_expression(config[CONF_COMMAND_HEAT])
        cg.add(var.set_command_heat(args))
    if CONF_COMMAND_FAN_ONLY in config:
        args = await command_hex_expression(config[CONF_COMMAND_FAN_ONLY])
        cg.add(var.set_command_fan_only(args))     
    if CONF_COMMAND_DRY in config:
        args = await command_hex_expression(config[CONF_COMMAND_DRY])
        cg.add(var.set_command_dry(args))        
    if CONF_COMMAND_AUTO in config:
        args = await command_hex_expression(config[CONF_COMMAND_AUTO])
        cg.add(var.set_command_auto(args))
    if CONF_COMMAND_SWING_OFF in config:
        args = await command_hex_expression(config[CONF_COMMAND_SWING_OFF])
        cg.add(var.set_command_swing_off(args))
    if CONF_COMMAND_SWING_BOTH in config:
        args = await command_hex_expression(config[CONF_COMMAND_SWING_BOTH])
        cg.add(var.set_command_swing_both(args))
    if CONF_COMMAND_SWING_VERTICAL in config:
        args = await command_hex_expression(config[CONF_COMMAND_SWING_VERTICAL])
        cg.add(var.set_command_swing_vertical(args))
    if CONF_COMMAND_SWING_HORIZONTAL in config:
        args = await command_hex_expression(config[CONF_COMMAND_SWING_HORIZONTAL])
        cg.add(var.set_command_swing_horizontal(args))
    if CONF_COMMAND_PRESET_NONE in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_NONE])
        cg.add(var.set_command_preset_none(args))
    if CONF_COMMAND_PRESET_HOME in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_HOME])
        cg.add(var.set_command_preset_home(args))
    if CONF_COMMAND_PRESET_AWAY in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_AWAY])
        cg.add(var.set_command_preset_away(args))
    if CONF_COMMAND_PRESET_BOOST in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_BOOST])
        cg.add(var.set_command_preset_boost(args))
    if CONF_COMMAND_PRESET_COMFORT in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_COMFORT])
        cg.add(var.set_command_preset_comfort(args))
    if CONF_COMMAND_PRESET_ECO in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_ECO])
        cg.add(var.set_command_preset_eco(args))
    if CONF_COMMAND_PRESET_SLEEP in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_SLEEP])
        cg.add(var.set_command_preset_sleep(args))
    if CONF_COMMAND_PRESET_ACTIVITY in config:
        args = await command_hex_expression(config[CONF_COMMAND_PRESET_ACTIVITY])
        cg.add(var.set_command_preset_activity(args))

