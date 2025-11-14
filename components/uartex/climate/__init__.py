import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, uartex, sensor
from esphome.const import CONF_ID, CONF_SENSOR, CONF_CUSTOM_FAN_MODE, CONF_CUSTOM_PRESET
from .. import uartex_ns, UARTExDevice, \
    state_schema, state_num_schema, state_hex_expression, state_num_expression, state_string_expression, \
    command_hex_schema, command_expression, command_float_expression, command_string_expression, validate_version
from ..const import CONF_STATE_TEMPERATURE_CURRENT, CONF_STATE_TEMPERATURE_TARGET, CONF_STATE_HUMIDITY_CURRENT, CONF_STATE_HUMIDITY_TARGET, \
    CONF_STATE_ON, CONF_STATE_AUTO, CONF_STATE_HEAT, CONF_STATE_COOL, CONF_STATE_FAN_ONLY, CONF_STATE_DRY, CONF_STATE_SWING_OFF, CONF_STATE_SWING_BOTH, CONF_STATE_SWING_VERTICAL, CONF_STATE_SWING_HORIZONTAL, \
    CONF_COMMAND_ON, CONF_COMMAND_AUTO, CONF_COMMAND_HEAT, CONF_COMMAND_COOL, CONF_COMMAND_FAN_ONLY, CONF_COMMAND_DRY, CONF_COMMAND_SWING_OFF, CONF_COMMAND_SWING_BOTH, CONF_COMMAND_SWING_VERTICAL, CONF_COMMAND_SWING_HORIZONTAL, \
    CONF_COMMAND_TEMPERATURE, CONF_COMMAND_HUMIDITY, CONF_LENGTH, CONF_PRECISION, CONF_COMMAND_PRESET_ECO, CONF_COMMAND_PRESET_SLEEP, CONF_COMMAND_PRESET_ACTIVITY, CONF_SIGNED, CONF_ENDIAN, CONF_DECODE, \
    CONF_COMMAND_OFF, CONF_COMMAND_PRESET_NONE, CONF_COMMAND_PRESET_HOME, CONF_COMMAND_PRESET_AWAY, CONF_COMMAND_PRESET_BOOST, CONF_COMMAND_PRESET_COMFORT, \
    CONF_STATE_PRESET_NONE, CONF_STATE_PRESET_HOME, CONF_STATE_PRESET_AWAY, CONF_STATE_PRESET_BOOST, CONF_STATE_PRESET_COMFORT, CONF_STATE_PRESET_ECO, CONF_STATE_PRESET_SLEEP, CONF_STATE_PRESET_ACTIVITY, \
    CONF_STATE_FAN_ON, CONF_STATE_FAN_OFF, CONF_STATE_FAN_AUTO, CONF_STATE_FAN_LOW, CONF_STATE_FAN_MEDIUM, CONF_STATE_FAN_HIGH, CONF_STATE_FAN_MIDDLE, CONF_STATE_FAN_FOCUS, CONF_STATE_FAN_DIFFUSE, CONF_STATE_FAN_QUIET, \
    CONF_COMMAND_FAN_ON, CONF_COMMAND_FAN_OFF, CONF_COMMAND_FAN_AUTO, CONF_COMMAND_FAN_LOW, CONF_COMMAND_FAN_MEDIUM, CONF_COMMAND_FAN_HIGH, CONF_COMMAND_FAN_MIDDLE, CONF_COMMAND_FAN_FOCUS, CONF_COMMAND_FAN_DIFFUSE, CONF_COMMAND_FAN_QUIET, \
    CONF_COMMAND_CUSTOM_FAN, CONF_COMMAND_CUSTOM_PRESET, CONF_STATE_CUSTOM_FAN, CONF_STATE_CUSTOM_PRESET, \
    CONF_STATE_ACTION_COOLING, CONF_STATE_ACTION_HEATING, CONF_STATE_ACTION_IDLE, CONF_STATE_ACTION_DRYING, CONF_STATE_ACTION_FAN
    
DEPENDENCIES = ['uartex']
UARTExClimate = uartex_ns.class_('UARTExClimate', climate.Climate, UARTExDevice)

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

CONFIG_SCHEMA = cv.All(climate.climate_schema(UARTExClimate).extend({
    cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
    cv.Optional(CONF_CUSTOM_FAN_MODE): validate_custom_modes,
    cv.Optional(CONF_CUSTOM_PRESET): validate_custom_modes,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE_TEMPERATURE_CURRENT): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_TEMPERATURE_TARGET): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_HUMIDITY_CURRENT): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_HUMIDITY_TARGET): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_COOL): state_schema,
    cv.Optional(CONF_STATE_HEAT): state_schema,
    cv.Optional(CONF_STATE_FAN_ONLY): state_schema,
    cv.Optional(CONF_STATE_DRY): state_schema,
    cv.Optional(CONF_STATE_AUTO): state_schema,
    cv.Optional(CONF_STATE_ACTION_COOLING): state_schema,
    cv.Optional(CONF_STATE_ACTION_HEATING): state_schema,
    cv.Optional(CONF_STATE_ACTION_IDLE): state_schema,
    cv.Optional(CONF_STATE_ACTION_DRYING): state_schema,
    cv.Optional(CONF_STATE_ACTION_FAN): state_schema,
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
    cv.Optional(CONF_COMMAND_TEMPERATURE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_HUMIDITY): cv.templatable(command_hex_schema),
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
    cv.Optional(CONF_COMMAND_CUSTOM_FAN): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_CUSTOM_PRESET): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Climate do not support command_on!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Climate do not support state_on!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_exactly_one_key(CONF_SENSOR, CONF_STATE_TEMPERATURE_CURRENT), validate_version)




async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_CUSTOM_FAN_MODE in config:
        cg.add(var.set_custom_fan_modes(config[CONF_CUSTOM_FAN_MODE]))

    if CONF_CUSTOM_PRESET in config:
        cg.add(var.set_custom_preset_modes(config[CONF_CUSTOM_PRESET]))

    if CONF_STATE_TEMPERATURE_TARGET in config:
        state = await state_num_expression(config[CONF_STATE_TEMPERATURE_TARGET])
        cg.add(var.set_state(CONF_STATE_TEMPERATURE_TARGET, state))

    if CONF_STATE_HUMIDITY_TARGET in config:
        state = await state_num_expression(config[CONF_STATE_HUMIDITY_TARGET])
        cg.add(var.set_state(CONF_STATE_HUMIDITY_TARGET, state))

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_STATE_TEMPERATURE_CURRENT in config:
        state = await state_num_expression(config[CONF_STATE_TEMPERATURE_CURRENT])
        cg.add(var.set_state(CONF_STATE_TEMPERATURE_CURRENT, state))

    if CONF_STATE_HUMIDITY_CURRENT in config:
        state = await state_num_expression(config[CONF_STATE_HUMIDITY_CURRENT])
        cg.add(var.set_state(CONF_STATE_HUMIDITY_CURRENT, state))

    if CONF_STATE_COOL in config:
        state = state_hex_expression(config[CONF_STATE_COOL])
        cg.add(var.set_state(CONF_STATE_COOL, state))

    if CONF_STATE_HEAT in config:
        state = state_hex_expression(config[CONF_STATE_HEAT])
        cg.add(var.set_state(CONF_STATE_HEAT, state))

    if CONF_STATE_FAN_ONLY in config:
        state = state_hex_expression(config[CONF_STATE_FAN_ONLY])
        cg.add(var.set_state(CONF_STATE_FAN_ONLY, state))

    if CONF_STATE_DRY in config:
        state = state_hex_expression(config[CONF_STATE_DRY])
        cg.add(var.set_state(CONF_STATE_DRY, state))

    if CONF_STATE_AUTO in config:
        state = state_hex_expression(config[CONF_STATE_AUTO])
        cg.add(var.set_state(CONF_STATE_AUTO, state))

    if CONF_STATE_ACTION_COOLING in config:
        state = state_hex_expression(config[CONF_STATE_ACTION_COOLING])
        cg.add(var.set_state(CONF_STATE_ACTION_COOLING, state))

    if CONF_STATE_ACTION_HEATING in config:
        state = state_hex_expression(config[CONF_STATE_ACTION_HEATING])
        cg.add(var.set_state(CONF_STATE_ACTION_HEATING, state))

    if CONF_STATE_ACTION_IDLE in config:
        state = state_hex_expression(config[CONF_STATE_ACTION_IDLE])
        cg.add(var.set_state(CONF_STATE_ACTION_IDLE, state))

    if CONF_STATE_ACTION_DRYING in config:
        state = state_hex_expression(config[CONF_STATE_ACTION_DRYING])
        cg.add(var.set_state(CONF_STATE_ACTION_DRYING, state))
        
    if CONF_STATE_ACTION_FAN in config:
        state = state_hex_expression(config[CONF_STATE_ACTION_FAN])
        cg.add(var.set_state(CONF_STATE_ACTION_FAN, state))

    if CONF_STATE_SWING_OFF in config:
        state = state_hex_expression(config[CONF_STATE_SWING_OFF])
        cg.add(var.set_state(CONF_STATE_SWING_OFF, state))

    if CONF_STATE_SWING_BOTH in config:
        state = state_hex_expression(config[CONF_STATE_SWING_BOTH])
        cg.add(var.set_state(CONF_STATE_SWING_BOTH, state))

    if CONF_STATE_SWING_VERTICAL in config:
        state = state_hex_expression(config[CONF_STATE_SWING_VERTICAL])
        cg.add(var.set_state(CONF_STATE_SWING_VERTICAL, state))

    if CONF_STATE_SWING_HORIZONTAL in config:
        state = state_hex_expression(config[CONF_STATE_SWING_HORIZONTAL])
        cg.add(var.set_state(CONF_STATE_SWING_HORIZONTAL, state))

    if CONF_STATE_FAN_ON in config:
        state = state_hex_expression(config[CONF_STATE_FAN_ON])
        cg.add(var.set_state(CONF_STATE_FAN_ON, state))

    if CONF_STATE_FAN_OFF in config:
        state = state_hex_expression(config[CONF_STATE_FAN_OFF])
        cg.add(var.set_state(CONF_STATE_FAN_OFF, state))

    if CONF_STATE_FAN_AUTO in config:
        state = state_hex_expression(config[CONF_STATE_FAN_AUTO])
        cg.add(var.set_state(CONF_STATE_FAN_AUTO, state))

    if CONF_STATE_FAN_LOW in config:
        state = state_hex_expression(config[CONF_STATE_FAN_LOW])
        cg.add(var.set_state(CONF_STATE_FAN_LOW, state))

    if CONF_STATE_FAN_MEDIUM in config:
        state = state_hex_expression(config[CONF_STATE_FAN_MEDIUM])
        cg.add(var.set_state(CONF_STATE_FAN_MEDIUM, state))

    if CONF_STATE_FAN_HIGH in config:
        state = state_hex_expression(config[CONF_STATE_FAN_HIGH])
        cg.add(var.set_state(CONF_STATE_FAN_HIGH, state))

    if CONF_STATE_FAN_MIDDLE in config:
        state = state_hex_expression(config[CONF_STATE_FAN_MIDDLE])
        cg.add(var.set_state(CONF_STATE_FAN_MIDDLE, state))

    if CONF_STATE_FAN_FOCUS in config:
        state = state_hex_expression(config[CONF_STATE_FAN_FOCUS])
        cg.add(var.set_state(CONF_STATE_FAN_FOCUS, state))

    if CONF_STATE_FAN_DIFFUSE in config:
        state = state_hex_expression(config[CONF_STATE_FAN_DIFFUSE])
        cg.add(var.set_state(CONF_STATE_FAN_DIFFUSE, state))

    if CONF_STATE_FAN_QUIET in config:
        state = state_hex_expression(config[CONF_STATE_FAN_QUIET])
        cg.add(var.set_state(CONF_STATE_FAN_QUIET, state))

    if CONF_STATE_PRESET_NONE in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_NONE])
        cg.add(var.set_state(CONF_STATE_PRESET_NONE, state))

    if CONF_STATE_PRESET_HOME in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_HOME])
        cg.add(var.set_state(CONF_STATE_PRESET_HOME, state))

    if CONF_STATE_PRESET_AWAY in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_AWAY])
        cg.add(var.set_state(CONF_STATE_PRESET_AWAY, state))

    if CONF_STATE_PRESET_BOOST in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_BOOST])
        cg.add(var.set_state(CONF_STATE_PRESET_BOOST, state))

    if CONF_STATE_PRESET_COMFORT in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_COMFORT])
        cg.add(var.set_state(CONF_STATE_PRESET_COMFORT, state))

    if CONF_STATE_PRESET_ECO in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_ECO])
        cg.add(var.set_state(CONF_STATE_PRESET_ECO, state))

    if CONF_STATE_PRESET_SLEEP in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_SLEEP])
        cg.add(var.set_state(CONF_STATE_PRESET_SLEEP, state))

    if CONF_STATE_PRESET_ACTIVITY in config:
        state = state_hex_expression(config[CONF_STATE_PRESET_ACTIVITY])
        cg.add(var.set_state(CONF_STATE_PRESET_ACTIVITY, state))

    if CONF_STATE_CUSTOM_FAN in config:
        state = await state_string_expression(config[CONF_STATE_CUSTOM_FAN])
        cg.add(var.set_state(CONF_STATE_CUSTOM_FAN, state))

    if CONF_STATE_CUSTOM_PRESET in config:
        state = await state_string_expression(config[CONF_STATE_CUSTOM_PRESET])
        cg.add(var.set_state(CONF_STATE_CUSTOM_PRESET, state))

    if CONF_COMMAND_TEMPERATURE in config:
        command = await command_float_expression(config[CONF_COMMAND_TEMPERATURE])
        cg.add(var.set_command(CONF_COMMAND_TEMPERATURE, command))

    if CONF_COMMAND_HUMIDITY in config:
        command = await command_float_expression(config[CONF_COMMAND_HUMIDITY])
        cg.add(var.set_command(CONF_COMMAND_HUMIDITY, command))

    if CONF_COMMAND_COOL in config:
        command = await command_expression(config[CONF_COMMAND_COOL])
        cg.add(var.set_command(CONF_COMMAND_COOL, command))

    if CONF_COMMAND_HEAT in config:
        command = await command_expression(config[CONF_COMMAND_HEAT])
        cg.add(var.set_command(CONF_COMMAND_HEAT, command))

    if CONF_COMMAND_FAN_ONLY in config:
        command = await command_expression(config[CONF_COMMAND_FAN_ONLY])
        cg.add(var.set_command(CONF_COMMAND_FAN_ONLY, command))

    if CONF_COMMAND_DRY in config:
        command = await command_expression(config[CONF_COMMAND_DRY])
        cg.add(var.set_command(CONF_COMMAND_DRY, command))

    if CONF_COMMAND_AUTO in config:
        command = await command_expression(config[CONF_COMMAND_AUTO])
        cg.add(var.set_command(CONF_COMMAND_AUTO, command))

    if CONF_COMMAND_SWING_OFF in config:
        command = await command_expression(config[CONF_COMMAND_SWING_OFF])
        cg.add(var.set_command(CONF_COMMAND_SWING_OFF, command))

    if CONF_COMMAND_SWING_BOTH in config:
        command = await command_expression(config[CONF_COMMAND_SWING_BOTH])
        cg.add(var.set_command(CONF_COMMAND_SWING_BOTH, command))

    if CONF_COMMAND_SWING_VERTICAL in config:
        command = await command_expression(config[CONF_COMMAND_SWING_VERTICAL])
        cg.add(var.set_command(CONF_COMMAND_SWING_VERTICAL, command))

    if CONF_COMMAND_SWING_HORIZONTAL in config:
        command = await command_expression(config[CONF_COMMAND_SWING_HORIZONTAL])
        cg.add(var.set_command(CONF_COMMAND_SWING_HORIZONTAL, command))

    if CONF_COMMAND_FAN_ON in config:
        command = await command_expression(config[CONF_COMMAND_FAN_ON])
        cg.add(var.set_command(CONF_COMMAND_FAN_ON, command))

    if CONF_COMMAND_FAN_OFF in config:
        command = await command_expression(config[CONF_COMMAND_FAN_OFF])
        cg.add(var.set_command(CONF_COMMAND_FAN_OFF, command))

    if CONF_COMMAND_FAN_AUTO in config:
        command = await command_expression(config[CONF_COMMAND_FAN_AUTO])
        cg.add(var.set_command(CONF_COMMAND_FAN_AUTO, command))

    if CONF_COMMAND_FAN_LOW in config:
        command = await command_expression(config[CONF_COMMAND_FAN_LOW])
        cg.add(var.set_command(CONF_COMMAND_FAN_LOW, command))

    if CONF_COMMAND_FAN_MEDIUM in config:
        command = await command_expression(config[CONF_COMMAND_FAN_MEDIUM])
        cg.add(var.set_command(CONF_COMMAND_FAN_MEDIUM, command))

    if CONF_COMMAND_FAN_HIGH in config:
        command = await command_expression(config[CONF_COMMAND_FAN_HIGH])
        cg.add(var.set_command(CONF_COMMAND_FAN_HIGH, command))

    if CONF_COMMAND_FAN_MIDDLE in config:
        command = await command_expression(config[CONF_COMMAND_FAN_MIDDLE])
        cg.add(var.set_command(CONF_COMMAND_FAN_MIDDLE, command))

    if CONF_COMMAND_FAN_FOCUS in config:
        command = await command_expression(config[CONF_COMMAND_FAN_FOCUS])
        cg.add(var.set_command(CONF_COMMAND_FAN_FOCUS, command))

    if CONF_COMMAND_FAN_DIFFUSE in config:
        command = await command_expression(config[CONF_COMMAND_FAN_DIFFUSE])
        cg.add(var.set_command(CONF_COMMAND_FAN_DIFFUSE, command))

    if CONF_COMMAND_FAN_QUIET in config:
        command = await command_expression(config[CONF_COMMAND_FAN_QUIET])
        cg.add(var.set_command(CONF_COMMAND_FAN_QUIET, command))

    if CONF_COMMAND_PRESET_NONE in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_NONE])
        cg.add(var.set_command(CONF_COMMAND_PRESET_NONE, command))

    if CONF_COMMAND_PRESET_HOME in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_HOME])
        cg.add(var.set_command(CONF_COMMAND_PRESET_HOME, command))

    if CONF_COMMAND_PRESET_AWAY in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_AWAY])
        cg.add(var.set_command(CONF_COMMAND_PRESET_AWAY, command))

    if CONF_COMMAND_PRESET_BOOST in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_BOOST])
        cg.add(var.set_command(CONF_COMMAND_PRESET_BOOST, command))

    if CONF_COMMAND_PRESET_COMFORT in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_COMFORT])
        cg.add(var.set_command(CONF_COMMAND_PRESET_COMFORT, command))

    if CONF_COMMAND_PRESET_ECO in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_ECO])
        cg.add(var.set_command(CONF_COMMAND_PRESET_ECO, command))

    if CONF_COMMAND_PRESET_SLEEP in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_SLEEP])
        cg.add(var.set_command(CONF_COMMAND_PRESET_SLEEP, command))
        
    if CONF_COMMAND_PRESET_ACTIVITY in config:
        command = await command_expression(config[CONF_COMMAND_PRESET_ACTIVITY])
        cg.add(var.set_command(CONF_COMMAND_PRESET_ACTIVITY, command))

    if CONF_COMMAND_CUSTOM_FAN in config:
        command = await command_string_expression(config[CONF_COMMAND_CUSTOM_FAN])
        cg.add(var.set_command(CONF_COMMAND_CUSTOM_FAN, command))

    if CONF_COMMAND_CUSTOM_PRESET in config:
        command = await command_string_expression(config[CONF_COMMAND_CUSTOM_PRESET])
        cg.add(var.set_command(CONF_COMMAND_CUSTOM_PRESET, command))