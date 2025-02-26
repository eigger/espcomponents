import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uartex, sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_OFFSET, CONF_CUSTOM_FAN_MODE, CONF_CUSTOM_PRESET
from .. import uartex_ns, cmd_t, uint8_ptr_const, uint16_const, \
    command_expression, state_schema, state_hex_expression, command_hex_schema, state_num_schema
from ..const import CONF_STATE_TEMPERATURE_CURRENT, CONF_STATE_TEMPERATURE_TARGET, CONF_STATE_HUMIDITY_CURRENT, CONF_STATE_HUMIDITY_TARGET, \
    CONF_STATE_ON, CONF_STATE_AUTO, CONF_STATE_HEAT, CONF_STATE_COOL, CONF_STATE_FAN_ONLY, CONF_STATE_DRY, CONF_STATE_SWING_OFF, CONF_STATE_SWING_BOTH, CONF_STATE_SWING_VERTICAL, CONF_STATE_SWING_HORIZONTAL, \
    CONF_COMMAND_ON, CONF_COMMAND_AUTO, CONF_COMMAND_HEAT, CONF_COMMAND_COOL, CONF_COMMAND_FAN_ONLY, CONF_COMMAND_DRY, CONF_COMMAND_SWING_OFF, CONF_COMMAND_SWING_BOTH, CONF_COMMAND_SWING_VERTICAL, CONF_COMMAND_SWING_HORIZONTAL, \
    CONF_COMMAND_TEMPERATURE, CONF_COMMAND_HUMIDITY, CONF_LENGTH, CONF_PRECISION, CONF_COMMAND_PRESET_ECO, CONF_COMMAND_PRESET_SLEEP, CONF_COMMAND_PRESET_ACTIVITY, \
    CONF_COMMAND_OFF, CONF_COMMAND_PRESET_NONE, CONF_COMMAND_PRESET_HOME, CONF_COMMAND_PRESET_AWAY, CONF_COMMAND_PRESET_BOOST, CONF_COMMAND_PRESET_COMFORT, \
    CONF_STATE_PRESET_NONE, CONF_STATE_PRESET_HOME, CONF_STATE_PRESET_AWAY, CONF_STATE_PRESET_BOOST, CONF_STATE_PRESET_COMFORT, CONF_STATE_PRESET_ECO, CONF_STATE_PRESET_SLEEP, CONF_STATE_PRESET_ACTIVITY, \
    CONF_STATE_FAN_ON, CONF_STATE_FAN_OFF, CONF_STATE_FAN_AUTO, CONF_STATE_FAN_LOW, CONF_STATE_FAN_MEDIUM, CONF_STATE_FAN_HIGH, CONF_STATE_FAN_MIDDLE, CONF_STATE_FAN_FOCUS, CONF_STATE_FAN_DIFFUSE, CONF_STATE_FAN_QUIET, \
    CONF_COMMAND_FAN_ON, CONF_COMMAND_FAN_OFF, CONF_COMMAND_FAN_AUTO, CONF_COMMAND_FAN_LOW, CONF_COMMAND_FAN_MEDIUM, CONF_COMMAND_FAN_HIGH, CONF_COMMAND_FAN_MIDDLE, CONF_COMMAND_FAN_FOCUS, CONF_COMMAND_FAN_DIFFUSE, CONF_COMMAND_FAN_QUIET, \
    CONF_COMMAND_CUSTOM_FAN, CONF_COMMAND_CUSTOM_PRESET, CONF_STATE_CUSTOM_FAN, CONF_STATE_CUSTOM_PRESET
    
AUTO_LOAD = ['sensor']
DEPENDENCIES = ['uartex']
UARTExClimate = uartex_ns.class_('UARTExClimate', climate.Climate, cg.Component)


_CUSTOM_MODES_SCHEMA = cv.All(
    cv.ensure_list(cv.string_strict),
    cv.Length(min=1),
)

def validate_custom_modes(value):
    # Check against defined schema
    value = _CUSTOM_MODES_SCHEMA(value)

    # Ensure custom names are unique
    errors = []
    customs = set()
    for i, custom in enumerate(value):
        # If name does not exist yet add it
        if custom not in customs:
            customs.add(custom)
            continue

        # Otherwise it's an error
        errors.append(
            cv.Invalid(
                f"Found duplicate custom name '{custom}'. Presets must have unique names.",
                [i],
            )
        )

    if errors:
        raise cv.MultipleInvalid(errors)

    return value

CONFIG_SCHEMA = cv.All(climate.CLIMATE_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExClimate),
    cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_STATE_TEMPERATURE_CURRENT): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_TEMPERATURE_TARGET): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_HUMIDITY_CURRENT): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_HUMIDITY_TARGET): cv.templatable(state_num_schema),
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
    cv.Optional(CONF_STATE_CUSTOM_FAN): cv.returning_lambda,
    cv.Optional(CONF_STATE_CUSTOM_PRESET): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_TEMPERATURE): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_HUMIDITY): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_COOL): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_HEAT): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_ONLY): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_DRY): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_AUTO): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_SWING_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_SWING_BOTH): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_SWING_VERTICAL): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_SWING_HORIZONTAL): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_ON): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_AUTO): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_LOW): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_MEDIUM): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_HIGH): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_MIDDLE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_FOCUS): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_DIFFUSE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_FAN_QUIET): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_NONE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_HOME): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_AWAY): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_BOOST): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_COMFORT): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_ECO): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_SLEEP): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PRESET_ACTIVITY): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_CUSTOM_FAN): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_CUSTOM_PRESET): cv.returning_lambda,
    cv.Optional(CONF_CUSTOM_FAN_MODE): validate_custom_modes,
    cv.Optional(CONF_CUSTOM_PRESET): validate_custom_modes,
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

    if CONF_CUSTOM_FAN_MODE in config:
        cg.add(var.set_custom_fan_modes(config[CONF_CUSTOM_FAN_MODE]))

    if CONF_CUSTOM_PRESET in config:
        cg.add(var.set_custom_preset_modes(config[CONF_CUSTOM_PRESET]))

    if CONF_STATE_TEMPERATURE_TARGET in config:
        state = config[CONF_STATE_TEMPERATURE_TARGET]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state(CONF_STATE_TEMPERATURE_TARGET, templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state(CONF_STATE_TEMPERATURE_TARGET, args))

    if CONF_STATE_HUMIDITY_TARGET in config:
        state = config[CONF_STATE_HUMIDITY_TARGET]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state(CONF_STATE_HUMIDITY_TARGET, templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state(CONF_STATE_HUMIDITY_TARGET, args))

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_STATE_TEMPERATURE_CURRENT in config:
        state = config[CONF_STATE_TEMPERATURE_CURRENT]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state(CONF_STATE_TEMPERATURE_CURRENT, templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state(CONF_STATE_TEMPERATURE_CURRENT, args))

    if CONF_STATE_HUMIDITY_CURRENT in config:
        state = config[CONF_STATE_HUMIDITY_CURRENT]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state(CONF_STATE_HUMIDITY_CURRENT, templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
            cg.add(var.set_state(CONF_STATE_HUMIDITY_CURRENT, args))

    if CONF_STATE_COOL in config:
        args = state_hex_expression(config[CONF_STATE_COOL])
        cg.add(var.set_state(CONF_STATE_COOL, args))

    if CONF_STATE_HEAT in config:
        args = state_hex_expression(config[CONF_STATE_HEAT])
        cg.add(var.set_state(CONF_STATE_HEAT, args))

    if CONF_STATE_FAN_ONLY in config:
        args = state_hex_expression(config[CONF_STATE_FAN_ONLY])
        cg.add(var.set_state(CONF_STATE_FAN_ONLY, args))

    if CONF_STATE_DRY in config:
        args = state_hex_expression(config[CONF_STATE_DRY])
        cg.add(var.set_state(CONF_STATE_DRY, args))

    if CONF_STATE_AUTO in config:
        args = state_hex_expression(config[CONF_STATE_AUTO])
        cg.add(var.set_state(CONF_STATE_AUTO, args))

    if CONF_STATE_SWING_OFF in config:
        args = state_hex_expression(config[CONF_STATE_SWING_OFF])
        cg.add(var.set_state(CONF_STATE_SWING_OFF, args))

    if CONF_STATE_SWING_BOTH in config:
        args = state_hex_expression(config[CONF_STATE_SWING_BOTH])
        cg.add(var.set_state(CONF_STATE_SWING_BOTH, args))

    if CONF_STATE_SWING_VERTICAL in config:
        args = state_hex_expression(config[CONF_STATE_SWING_VERTICAL])
        cg.add(var.set_state(CONF_STATE_SWING_VERTICAL, args))

    if CONF_STATE_SWING_HORIZONTAL in config:
        args = state_hex_expression(config[CONF_STATE_SWING_HORIZONTAL])
        cg.add(var.set_state(CONF_STATE_SWING_HORIZONTAL, args))

    if CONF_STATE_FAN_ON in config:
        args = state_hex_expression(config[CONF_STATE_FAN_ON])
        cg.add(var.set_state(CONF_STATE_FAN_ON, args))

    if CONF_STATE_FAN_OFF in config:
        args = state_hex_expression(config[CONF_STATE_FAN_OFF])
        cg.add(var.set_state(CONF_STATE_FAN_OFF, args))

    if CONF_STATE_FAN_AUTO in config:
        args = state_hex_expression(config[CONF_STATE_FAN_AUTO])
        cg.add(var.set_state(CONF_STATE_FAN_AUTO, args))

    if CONF_STATE_FAN_LOW in config:
        args = state_hex_expression(config[CONF_STATE_FAN_LOW])
        cg.add(var.set_state(CONF_STATE_FAN_LOW, args))

    if CONF_STATE_FAN_MEDIUM in config:
        args = state_hex_expression(config[CONF_STATE_FAN_MEDIUM])
        cg.add(var.set_state(CONF_STATE_FAN_MEDIUM, args))

    if CONF_STATE_FAN_HIGH in config:
        args = state_hex_expression(config[CONF_STATE_FAN_HIGH])
        cg.add(var.set_state(CONF_STATE_FAN_HIGH, args))

    if CONF_STATE_FAN_MIDDLE in config:
        args = state_hex_expression(config[CONF_STATE_FAN_MIDDLE])
        cg.add(var.set_state(CONF_STATE_FAN_MIDDLE, args))

    if CONF_STATE_FAN_FOCUS in config:
        args = state_hex_expression(config[CONF_STATE_FAN_FOCUS])
        cg.add(var.set_state(CONF_STATE_FAN_FOCUS, args))

    if CONF_STATE_FAN_DIFFUSE in config:
        args = state_hex_expression(config[CONF_STATE_FAN_DIFFUSE])
        cg.add(var.set_state(CONF_STATE_FAN_DIFFUSE, args))

    if CONF_STATE_FAN_QUIET in config:
        args = state_hex_expression(config[CONF_STATE_FAN_QUIET])
        cg.add(var.set_state(CONF_STATE_FAN_QUIET, args))

    if CONF_STATE_PRESET_NONE in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_NONE])
        cg.add(var.set_state(CONF_STATE_PRESET_NONE, args))

    if CONF_STATE_PRESET_HOME in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_HOME])
        cg.add(var.set_state(CONF_STATE_PRESET_HOME, args))

    if CONF_STATE_PRESET_AWAY in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_AWAY])
        cg.add(var.set_state(CONF_STATE_PRESET_AWAY, args))

    if CONF_STATE_PRESET_BOOST in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_BOOST])
        cg.add(var.set_state(CONF_STATE_PRESET_BOOST, args))

    if CONF_STATE_PRESET_COMFORT in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_COMFORT])
        cg.add(var.set_state(CONF_STATE_PRESET_COMFORT, args))

    if CONF_STATE_PRESET_ECO in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_ECO])
        cg.add(var.set_state(CONF_STATE_PRESET_ECO, args))

    if CONF_STATE_PRESET_SLEEP in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_SLEEP])
        cg.add(var.set_state(CONF_STATE_PRESET_SLEEP, args))

    if CONF_STATE_PRESET_ACTIVITY in config:
        args = state_hex_expression(config[CONF_STATE_PRESET_ACTIVITY])
        cg.add(var.set_state(CONF_STATE_PRESET_ACTIVITY, args))

    if CONF_STATE_CUSTOM_FAN in config:
        templ = await cg.templatable(config[CONF_STATE_CUSTOM_FAN], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.std_string)
        cg.add(var.set_state(CONF_STATE_CUSTOM_FAN, templ))

    if CONF_STATE_CUSTOM_PRESET in config:
        templ = await cg.templatable(config[CONF_STATE_CUSTOM_PRESET], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.std_string)
        cg.add(var.set_state(CONF_STATE_CUSTOM_PRESET, templ))

    if CONF_COMMAND_TEMPERATURE in config:
        templ = await cg.templatable(config[CONF_COMMAND_TEMPERATURE], [(cg.float_.operator('const'), 'x')], cmd_t)
        cg.add(var.set_command(CONF_COMMAND_TEMPERATURE, templ))

    if CONF_COMMAND_HUMIDITY in config:
        templ = await cg.templatable(config[CONF_COMMAND_HUMIDITY], [(cg.float_.operator('const'), 'x')], cmd_t)
        cg.add(var.set_command(CONF_COMMAND_HUMIDITY, templ))

    if CONF_COMMAND_COOL in config:
        args = await command_expression(config[CONF_COMMAND_COOL])
        cg.add(var.set_command(CONF_COMMAND_COOL, args))

    if CONF_COMMAND_HEAT in config:
        args = await command_expression(config[CONF_COMMAND_HEAT])
        cg.add(var.set_command(CONF_COMMAND_HEAT, args))

    if CONF_COMMAND_FAN_ONLY in config:
        args = await command_expression(config[CONF_COMMAND_FAN_ONLY])
        cg.add(var.set_command(CONF_COMMAND_FAN_ONLY, args))

    if CONF_COMMAND_DRY in config:
        args = await command_expression(config[CONF_COMMAND_DRY])
        cg.add(var.set_command(CONF_COMMAND_DRY, args))

    if CONF_COMMAND_AUTO in config:
        args = await command_expression(config[CONF_COMMAND_AUTO])
        cg.add(var.set_command(CONF_COMMAND_AUTO, args))

    if CONF_COMMAND_SWING_OFF in config:
        args = await command_expression(config[CONF_COMMAND_SWING_OFF])
        cg.add(var.set_command(CONF_COMMAND_SWING_OFF, args))

    if CONF_COMMAND_SWING_BOTH in config:
        args = await command_expression(config[CONF_COMMAND_SWING_BOTH])
        cg.add(var.set_command(CONF_COMMAND_SWING_BOTH, args))

    if CONF_COMMAND_SWING_VERTICAL in config:
        args = await command_expression(config[CONF_COMMAND_SWING_VERTICAL])
        cg.add(var.set_command(CONF_COMMAND_SWING_VERTICAL, args))

    if CONF_COMMAND_SWING_HORIZONTAL in config:
        args = await command_expression(config[CONF_COMMAND_SWING_HORIZONTAL])
        cg.add(var.set_command(CONF_COMMAND_SWING_HORIZONTAL, args))

    if CONF_COMMAND_FAN_ON in config:
        args = await command_expression(config[CONF_COMMAND_FAN_ON])
        cg.add(var.set_command(CONF_COMMAND_FAN_ON, args))

    if CONF_COMMAND_FAN_OFF in config:
        args = await command_expression(config[CONF_COMMAND_FAN_OFF])
        cg.add(var.set_command(CONF_COMMAND_FAN_OFF, args))

    if CONF_COMMAND_FAN_AUTO in config:
        args = await command_expression(config[CONF_COMMAND_FAN_AUTO])
        cg.add(var.set_command(CONF_COMMAND_FAN_AUTO, args))

    if CONF_COMMAND_FAN_LOW in config:
        args = await command_expression(config[CONF_COMMAND_FAN_LOW])
        cg.add(var.set_command(CONF_COMMAND_FAN_LOW, args))

    if CONF_COMMAND_FAN_MEDIUM in config:
        args = await command_expression(config[CONF_COMMAND_FAN_MEDIUM])
        cg.add(var.set_command(CONF_COMMAND_FAN_MEDIUM, args))

    if CONF_COMMAND_FAN_HIGH in config:
        args = await command_expression(config[CONF_COMMAND_FAN_HIGH])
        cg.add(var.set_command(CONF_COMMAND_FAN_HIGH, args))

    if CONF_COMMAND_FAN_MIDDLE in config:
        args = await command_expression(config[CONF_COMMAND_FAN_MIDDLE])
        cg.add(var.set_command(CONF_COMMAND_FAN_MIDDLE, args))

    if CONF_COMMAND_FAN_FOCUS in config:
        args = await command_expression(config[CONF_COMMAND_FAN_FOCUS])
        cg.add(var.set_command(CONF_COMMAND_FAN_FOCUS, args))

    if CONF_COMMAND_FAN_DIFFUSE in config:
        args = await command_expression(config[CONF_COMMAND_FAN_DIFFUSE])
        cg.add(var.set_command(CONF_COMMAND_FAN_DIFFUSE, args))

    if CONF_COMMAND_FAN_QUIET in config:
        args = await command_expression(config[CONF_COMMAND_FAN_QUIET])
        cg.add(var.set_command(CONF_COMMAND_FAN_QUIET, args))

    if CONF_COMMAND_PRESET_NONE in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_NONE])
        cg.add(var.set_command(CONF_COMMAND_PRESET_NONE, args))

    if CONF_COMMAND_PRESET_HOME in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_HOME])
        cg.add(var.set_command(CONF_COMMAND_PRESET_HOME, args))

    if CONF_COMMAND_PRESET_AWAY in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_AWAY])
        cg.add(var.set_command(CONF_COMMAND_PRESET_AWAY, args))

    if CONF_COMMAND_PRESET_BOOST in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_BOOST])
        cg.add(var.set_command(CONF_COMMAND_PRESET_BOOST, args))

    if CONF_COMMAND_PRESET_COMFORT in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_COMFORT])
        cg.add(var.set_command(CONF_COMMAND_PRESET_COMFORT, args))

    if CONF_COMMAND_PRESET_ECO in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_ECO])
        cg.add(var.set_command(CONF_COMMAND_PRESET_ECO, args))

    if CONF_COMMAND_PRESET_SLEEP in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_SLEEP])
        cg.add(var.set_command(CONF_COMMAND_PRESET_SLEEP, args))
        
    if CONF_COMMAND_PRESET_ACTIVITY in config:
        args = await command_expression(config[CONF_COMMAND_PRESET_ACTIVITY])
        cg.add(var.set_command(CONF_COMMAND_PRESET_ACTIVITY, args))

    if CONF_COMMAND_CUSTOM_FAN in config:
        templ = await cg.templatable(config[CONF_COMMAND_CUSTOM_FAN], [(cg.std_string.operator('const'), 'str')], cmd_t)
        cg.add(var.set_command(CONF_COMMAND_CUSTOM_FAN, templ))

    if CONF_COMMAND_CUSTOM_PRESET in config:
        templ = await cg.templatable(config[CONF_COMMAND_CUSTOM_PRESET], [(cg.std_string.operator('const'), 'str')], cmd_t)
        cg.add(var.set_command(CONF_COMMAND_CUSTOM_PRESET, templ))