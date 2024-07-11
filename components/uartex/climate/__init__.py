import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uartex, sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_OFFSET
from .. import uartex_ns, command_hex_schema, STATE_NUM_SCHEMA, cmd_t, uint8_ptr_const, uint16_const, uint8_const, \
    command_hex_expression, state_schema, state_hex_expression
from ..const import CONF_STATE_TEMPERATURE_CURRENT, CONF_STATE_TEMPERATURE_TARGET, CONF_STATE_HUMIDITY_CURRENT, CONF_STATE_HUMIDITY_TARGET, \
    CONF_STATE_ON, CONF_STATE_AUTO, CONF_STATE_HEAT, CONF_STATE_COOL, CONF_STATE_FAN_ONLY, CONF_STATE_DRY, CONF_STATE_SWING_OFF, CONF_STATE_SWING_BOTH, CONF_STATE_SWING_VERTICAL, CONF_STATE_SWING_HORIZONTAL, \
    CONF_COMMAND_ON, CONF_COMMAND_AUTO, CONF_COMMAND_HEAT, CONF_COMMAND_COOL, CONF_COMMAND_FAN_ONLY, CONF_COMMAND_DRY, CONF_COMMAND_SWING_OFF, CONF_COMMAND_SWING_BOTH, CONF_COMMAND_SWING_VERTICAL, CONF_COMMAND_SWING_HORIZONTAL, \
    CONF_COMMAND_TEMPERATURE, CONF_COMMAND_HUMIDITY, CONF_LENGTH, CONF_PRECISION, CONF_COMMAND_PRESET_ECO, CONF_COMMAND_PRESET_SLEEP, CONF_COMMAND_PRESET_ACTIVITY, \
    CONF_COMMAND_OFF, CONF_COMMAND_PRESET_NONE, CONF_COMMAND_PRESET_HOME, CONF_COMMAND_PRESET_AWAY, CONF_COMMAND_PRESET_BOOST, CONF_COMMAND_PRESET_COMFORT, \
    CONF_STATE_PRESET_NONE, CONF_STATE_PRESET_HOME, CONF_STATE_PRESET_AWAY, CONF_STATE_PRESET_BOOST, CONF_STATE_PRESET_COMFORT, CONF_STATE_PRESET_ECO, CONF_STATE_PRESET_SLEEP, CONF_STATE_PRESET_ACTIVITY, \
    CONF_STATE_FAN_ON, CONF_STATE_FAN_OFF, CONF_STATE_FAN_AUTO, CONF_STATE_FAN_LOW, CONF_STATE_FAN_MEDIUM, CONF_STATE_FAN_HIGH, CONF_STATE_FAN_MIDDLE, CONF_STATE_FAN_FOCUS, CONF_STATE_FAN_DIFFUSE, CONF_STATE_FAN_QUIET, \
    CONF_COMMAND_FAN_ON, CONF_COMMAND_FAN_OFF, CONF_COMMAND_FAN_AUTO, CONF_COMMAND_FAN_LOW, CONF_COMMAND_FAN_MEDIUM, CONF_COMMAND_FAN_HIGH, CONF_COMMAND_FAN_MIDDLE, CONF_COMMAND_FAN_FOCUS, CONF_COMMAND_FAN_DIFFUSE, CONF_COMMAND_FAN_QUIET
    

AUTO_LOAD = ['sensor']
DEPENDENCIES = ['uartex']

UARTExClimate = uartex_ns.class_('UARTExClimate', climate.Climate, cg.Component)
climate_t = uartex_ns.struct('climate_t')
climate_t_const = climate_t.operator('const')

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExClimate),
    cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_STATE_TEMPERATURE_CURRENT): cv.templatable(STATE_NUM_SCHEMA),
    cv.Optional(CONF_STATE_TEMPERATURE_TARGET): cv.templatable(STATE_NUM_SCHEMA),
    cv.Optional(CONF_STATE_HUMIDITY_CURRENT): cv.templatable(STATE_NUM_SCHEMA),
    cv.Optional(CONF_STATE_HUMIDITY_TARGET): cv.templatable(STATE_NUM_SCHEMA),
    cv.Optional(CONF_STATE_COOL): state_schema,
    cv.Optional(CONF_STATE_HEAT): state_schema,
    cv.Optional(CONF_STATE_FAN_ONLY): state_schema,
    cv.Optional(CONF_STATE_DRY): state_schema,
    cv.Optional(CONF_STATE_AUTO): state_schema,
    cv.Optional(CONF_STATE_SWING_OFF): state_schema,
    cv.Optional(CONF_STATE_SWING_BOTH): state_schema,
    cv.Optional(CONF_STATE_SWING_VERTICAL): state_schema,
    cv.Optional(CONF_STATE_SWING_HORIZONTAL): state_schema,
    cv.Optional(CONF_STATE_FAN_ON): state_schema,
    cv.Optional(CONF_STATE_FAN_OFF): state_schema,
    cv.Optional(CONF_STATE_FAN_AUTO): state_schema,
    cv.Optional(CONF_STATE_FAN_LOW): state_schema,
    cv.Optional(CONF_STATE_FAN_MEDIUM): state_schema,
    cv.Optional(CONF_STATE_FAN_HIGH): state_schema,
    cv.Optional(CONF_STATE_FAN_MIDDLE): state_schema,
    cv.Optional(CONF_STATE_FAN_FOCUS): state_schema,
    cv.Optional(CONF_STATE_FAN_DIFFUSE): state_schema,
    cv.Optional(CONF_STATE_FAN_QUIET): state_schema,
    cv.Optional(CONF_STATE_PRESET_NONE): state_schema,
    cv.Optional(CONF_STATE_PRESET_HOME): state_schema,
    cv.Optional(CONF_STATE_PRESET_AWAY): state_schema,
    cv.Optional(CONF_STATE_PRESET_BOOST): state_schema,
    cv.Optional(CONF_STATE_PRESET_COMFORT): state_schema,
    cv.Optional(CONF_STATE_PRESET_ECO): state_schema,
    cv.Optional(CONF_STATE_PRESET_SLEEP): state_schema,
    cv.Optional(CONF_STATE_PRESET_ACTIVITY): state_schema,
    cv.Optional(CONF_COMMAND_TEMPERATURE): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_HUMIDITY): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_COOL): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_HEAT): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_ONLY): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_DRY): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_AUTO): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_SWING_OFF): command_hex_schema,
    cv.Optional(CONF_COMMAND_SWING_BOTH): command_hex_schema,
    cv.Optional(CONF_COMMAND_SWING_VERTICAL): command_hex_schema,
    cv.Optional(CONF_COMMAND_SWING_HORIZONTAL): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_ON): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_OFF): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_AUTO): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_LOW): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_MEDIUM): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_HIGH): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_MIDDLE): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_FOCUS): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_DIFFUSE): command_hex_schema,
    cv.Optional(CONF_COMMAND_FAN_QUIET): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_NONE): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_HOME): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_AWAY): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_BOOST): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_COMFORT): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_ECO): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_SLEEP): command_hex_schema,
    cv.Optional(CONF_COMMAND_PRESET_ACTIVITY): command_hex_schema,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Climate do not support command_on!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Climate do not support state_on!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_exactly_one_key(CONF_SENSOR, CONF_STATE_TEMPERATURE_CURRENT))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await climate.register_climate(var, config)
    await uartex.register_uartex_device(var, config)
    if CONF_COMMAND_TEMPERATURE in config:
        templ = await cg.templatable(config[CONF_COMMAND_TEMPERATURE], [(cg.float_.operator('const'), 'x'), (climate_t_const, 'climate')], cmd_t)
        cg.add(var.set_command_temperature(templ))
    if CONF_COMMAND_HUMIDITY in config:
        templ = await cg.templatable(config[CONF_COMMAND_HUMIDITY], [(cg.float_.operator('const'), 'x'), (climate_t_const, 'climate')], cmd_t)
        cg.add(var.set_command_humidity(templ))
    if CONF_STATE_TEMPERATURE_TARGET in config:
        state = config[CONF_STATE_TEMPERATURE_TARGET]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state_target_temperature(templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state_target_temperature(args))
    if CONF_STATE_HUMIDITY_TARGET in config:
        state = config[CONF_STATE_HUMIDITY_TARGET]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state_target_humidity(templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state_target_humidity(args))
    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))
    if CONF_STATE_TEMPERATURE_CURRENT in config:
        state = config[CONF_STATE_TEMPERATURE_CURRENT]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state_current_temperature(templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state_current_temperature(args))
    if CONF_STATE_HUMIDITY_CURRENT in config:
        state = config[CONF_STATE_HUMIDITY_CURRENT]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state_current_humidity(templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state_current_humidity(args))
    if CONF_STATE_COOL in config:
        args = state_hex_expression(config[CONF_STATE_COOL])
        cg.add(var.set_state_cool(args))
    if CONF_STATE_HEAT in config:
        args = state_hex_expression(config[CONF_STATE_HEAT])
        cg.add(var.set_state_heat(args))
    if CONF_STATE_FAN_ONLY in config:
        args = state_hex_expression(config[CONF_STATE_FAN_ONLY])
        cg.add(var.set_state_fan_only(args))
    if CONF_STATE_DRY in config:
        args = state_hex_expression(config[CONF_STATE_DRY])
        cg.add(var.set_state_dry(args))
    if CONF_STATE_AUTO in config:
        args = state_hex_expression(config[CONF_STATE_AUTO])
        cg.add(var.set_state_auto(args))
    if CONF_STATE_SWING_OFF in config:
        args = state_hex_expression(config[CONF_STATE_SWING_OFF])
        cg.add(var.set_state_swing_off(args))
    if CONF_STATE_SWING_BOTH in config:
        args = state_hex_expression(config[CONF_STATE_SWING_BOTH])
        cg.add(var.set_state_swing_both(args))
    if CONF_STATE_SWING_VERTICAL in config:
        args = state_hex_expression(config[CONF_STATE_SWING_VERTICAL])
        cg.add(var.set_state_swing_vertical(args))
    if CONF_STATE_SWING_HORIZONTAL in config:
        args = state_hex_expression(config[CONF_STATE_SWING_HORIZONTAL])
        cg.add(var.set_state_swing_horizontal(args))
    if CONF_STATE_FAN_ON in config:
        args = state_hex_expression(config[CONF_STATE_FAN_ON])
        cg.add(var.set_state_fan_on(args))
    if CONF_STATE_FAN_OFF in config:
        args = state_hex_expression(config[CONF_STATE_FAN_OFF])
        cg.add(var.set_state_fan_off(args))
    if CONF_STATE_FAN_AUTO in config:
        args = state_hex_expression(config[CONF_STATE_FAN_AUTO])
        cg.add(var.set_state_fan_auto(args))
    if CONF_STATE_FAN_LOW in config:
        args = state_hex_expression(config[CONF_STATE_FAN_LOW])
        cg.add(var.set_state_fan_low(args))
    if CONF_STATE_FAN_MEDIUM in config:
        args = state_hex_expression(config[CONF_STATE_FAN_MEDIUM])
        cg.add(var.set_state_fan_medium(args))
    if CONF_STATE_FAN_HIGH in config:
        args = state_hex_expression(config[CONF_STATE_FAN_HIGH])
        cg.add(var.set_state_fan_high(args))
    if CONF_STATE_FAN_MIDDLE in config:
        args = state_hex_expression(config[CONF_STATE_FAN_MIDDLE])
        cg.add(var.set_state_fan_middle(args))
    if CONF_STATE_FAN_FOCUS in config:
        args = state_hex_expression(config[CONF_STATE_FAN_FOCUS])
        cg.add(var.set_state_fan_focus(args))
    if CONF_STATE_FAN_DIFFUSE in config:
        args = state_hex_expression(config[CONF_STATE_FAN_DIFFUSE])
        cg.add(var.set_state_fan_diffuse(args))
    if CONF_STATE_FAN_QUIET in config:
        args = state_hex_expression(config[CONF_STATE_FAN_QUIET])
        cg.add(var.set_state_fan_quiet(args))
    if CONF_STATE_PRESET_NONE in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_NONE])
        cg.add(var.set_state_preset_none(args))
    if CONF_STATE_PRESET_HOME in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_HOME])
        cg.add(var.set_state_preset_home(args))
    if CONF_STATE_PRESET_AWAY in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_AWAY])
        cg.add(var.set_state_preset_away(args))
    if CONF_STATE_PRESET_BOOST in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_BOOST])
        cg.add(var.set_state_preset_boost(args))
    if CONF_STATE_PRESET_COMFORT in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_COMFORT])
        cg.add(var.set_state_preset_comfort(args))
    if CONF_STATE_PRESET_ECO in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_ECO])
        cg.add(var.set_state_preset_eco(args))
    if CONF_STATE_PRESET_SLEEP in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_SLEEP])
        cg.add(var.set_state_preset_sleep(args))
    if CONF_STATE_PRESET_ACTIVITY in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_ACTIVITY])
        cg.add(var.set_state_preset_activity(args))
    if CONF_COMMAND_COOL in config:
        cmd = config[CONF_COMMAND_COOL]
        if cg.is_template(cmd):
            templ = await cg.templatable(config[CONF_COMMAND_COOL], [(climate_t_const, 'climate')], cmd_t)
            cg.add(var.set_command_cool(templ))
        else:
            args = command_hex_expression(config[CONF_COMMAND_COOL])
            cg.add(var.set_command_cool(args))
    if CONF_COMMAND_HEAT in config:
        cmd = config[CONF_COMMAND_HEAT]
        if cg.is_template(cmd):
            templ = await cg.templatable(config[CONF_COMMAND_HEAT], [(climate_t_const, 'climate')], cmd_t)
            cg.add(var.set_command_heat(templ))
        else:
            args = command_hex_expression(config[CONF_COMMAND_HEAT])
            cg.add(var.set_command_heat(args))
    if CONF_COMMAND_FAN_ONLY in config:
        cmd = config[CONF_COMMAND_FAN_ONLY]
        if cg.is_template(cmd):
            templ = await cg.templatable(config[CONF_COMMAND_FAN_ONLY], [(climate_t_const, 'climate')], cmd_t)
            cg.add(var.set_command_fan_only(templ))
        else:
            args = command_hex_expression(config[CONF_COMMAND_FAN_ONLY])
            cg.add(var.set_command_fan_only(args))     
    if CONF_COMMAND_DRY in config:
        cmd = config[CONF_COMMAND_DRY]
        if cg.is_template(cmd):
            templ = await cg.templatable(config[CONF_COMMAND_DRY], [(climate_t_const, 'climate')], cmd_t)
            cg.add(var.set_command_dry(templ))
        else:
            args = command_hex_expression(config[CONF_COMMAND_DRY])
            cg.add(var.set_command_dry(args))        
    if CONF_COMMAND_AUTO in config:
        cmd = config[CONF_COMMAND_AUTO]
        if cg.is_template(cmd):
            templ = await cg.templatable(config[CONF_COMMAND_AUTO], [(climate_t_const, 'climate')], cmd_t)
            cg.add(var.set_command_auto(templ))
        else:
            args = command_hex_expression(config[CONF_COMMAND_AUTO])
            cg.add(var.set_command_auto(args))
    if CONF_COMMAND_SWING_OFF in config:
        args = command_hex_expression(config[CONF_COMMAND_SWING_OFF])
        cg.add(var.set_command_swing_off(args))
    if CONF_COMMAND_SWING_BOTH in config:
        args = command_hex_expression(config[CONF_COMMAND_SWING_BOTH])
        cg.add(var.set_command_swing_both(args))
    if CONF_COMMAND_SWING_VERTICAL in config:
        args = command_hex_expression(config[CONF_COMMAND_SWING_VERTICAL])
        cg.add(var.set_command_swing_vertical(args))
    if CONF_COMMAND_SWING_HORIZONTAL in config:
        args = command_hex_expression(config[CONF_COMMAND_SWING_HORIZONTAL])
        cg.add(var.set_command_swing_horizontal(args))
    if CONF_COMMAND_FAN_ON in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_ON])
        cg.add(var.set_command_fan_on(args))
    if CONF_COMMAND_FAN_OFF in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_OFF])
        cg.add(var.set_command_fan_off(args))
    if CONF_COMMAND_FAN_AUTO in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_AUTO])
        cg.add(var.set_command_fan_auto(args))
    if CONF_COMMAND_FAN_LOW in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_LOW])
        cg.add(var.set_command_fan_low(args))
    if CONF_COMMAND_FAN_MEDIUM in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_MEDIUM])
        cg.add(var.set_command_fan_medium(args))
    if CONF_COMMAND_FAN_HIGH in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_HIGH])
        cg.add(var.set_command_fan_high(args))
    if CONF_COMMAND_FAN_MIDDLE in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_MIDDLE])
        cg.add(var.set_command_fan_middle(args))
    if CONF_COMMAND_FAN_FOCUS in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_FOCUS])
        cg.add(var.set_command_fan_focus(args))
    if CONF_COMMAND_FAN_DIFFUSE in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_DIFFUSE])
        cg.add(var.set_command_fan_diffuse(args))
    if CONF_COMMAND_FAN_QUIET in config:
        args = command_hex_expression(config[CONF_COMMAND_FAN_QUIET])
        cg.add(var.set_command_fan_quiet(args))
    if CONF_COMMAND_PRESET_NONE in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_NONE])
        cg.add(var.set_command_preset_none(args))
    if CONF_COMMAND_PRESET_HOME in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_HOME])
        cg.add(var.set_command_preset_home(args))
    if CONF_COMMAND_PRESET_AWAY in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_AWAY])
        cg.add(var.set_command_preset_away(args))
    if CONF_COMMAND_PRESET_BOOST in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_BOOST])
        cg.add(var.set_command_preset_boost(args))
    if CONF_COMMAND_PRESET_COMFORT in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_COMFORT])
        cg.add(var.set_command_preset_comfort(args))
    if CONF_COMMAND_PRESET_ECO in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_ECO])
        cg.add(var.set_command_preset_eco(args))
    if CONF_COMMAND_PRESET_SLEEP in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_SLEEP])
        cg.add(var.set_command_preset_sleep(args))
    if CONF_COMMAND_PRESET_ACTIVITY in config:
        args = command_hex_expression(config[CONF_COMMAND_PRESET_ACTIVITY])
        cg.add(var.set_command_preset_activity(args))

