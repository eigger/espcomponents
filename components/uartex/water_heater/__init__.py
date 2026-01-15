import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import water_heater, uartex, sensor
from esphome.const import CONF_ID, CONF_SENSOR
from .. import uartex_ns, UARTExDevice, \
    state_schema, state_num_schema, state_hex_expression, state_num_expression, \
    command_hex_schema, command_expression, command_float_expression
from ..const import CONF_STATE_TEMPERATURE_CURRENT, CONF_STATE_TEMPERATURE_TARGET, \
    CONF_STATE_ECO, CONF_STATE_ELECTRIC, CONF_STATE_PERFORMANCE, CONF_STATE_HIGH_DEMAND, CONF_STATE_HEAT_PUMP, CONF_STATE_GAS, CONF_STATE_AWAY_ON, CONF_STATE_AWAY_OFF, \
    CONF_COMMAND_TEMPERATURE, \
    CONF_COMMAND_ECO, CONF_COMMAND_ELECTRIC, CONF_COMMAND_PERFORMANCE, CONF_COMMAND_HIGH_DEMAND, CONF_COMMAND_HEAT_PUMP, CONF_COMMAND_GAS, \
    CONF_COMMAND_AWAY_ON, CONF_COMMAND_AWAY_OFF

DEPENDENCIES = ['uartex']
UARTExWaterHeater = uartex_ns.class_('UARTExWaterHeater', water_heater.WaterHeater, UARTExDevice)

CONFIG_SCHEMA = cv.All(water_heater.water_heater_schema(UARTExWaterHeater).extend({
    cv.Optional(CONF_SENSOR): cv.use_id(sensor.Sensor),
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE_TEMPERATURE_CURRENT): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_TEMPERATURE_TARGET): cv.templatable(state_num_schema),
    cv.Optional(CONF_STATE_ECO): state_schema,
    cv.Optional(CONF_STATE_ELECTRIC): state_schema,
    cv.Optional(CONF_STATE_PERFORMANCE): state_schema,
    cv.Optional(CONF_STATE_HIGH_DEMAND): state_schema,
    cv.Optional(CONF_STATE_HEAT_PUMP): state_schema,
    cv.Optional(CONF_STATE_GAS): state_schema,
    cv.Optional(CONF_STATE_AWAY_ON): state_schema,
    cv.Optional(CONF_STATE_AWAY_OFF): state_schema,
    cv.Optional(CONF_COMMAND_TEMPERATURE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ECO): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ELECTRIC): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PERFORMANCE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_HIGH_DEMAND): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_HEAT_PUMP): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_GAS): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_AWAY_ON): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_AWAY_OFF): cv.templatable(command_hex_schema),
}).extend(cv.COMPONENT_SCHEMA))


async def to_code(config):
    var = await water_heater.new_water_heater(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_SENSOR in config:
        sens = await cg.get_variable(config[CONF_SENSOR])
        cg.add(var.set_sensor(sens))

    if CONF_STATE_TEMPERATURE_CURRENT in config:
        state = await state_num_expression(config[CONF_STATE_TEMPERATURE_CURRENT])
        cg.add(var.set_state(CONF_STATE_TEMPERATURE_CURRENT, state))

    if CONF_STATE_TEMPERATURE_TARGET in config:
        state = await state_num_expression(config[CONF_STATE_TEMPERATURE_TARGET])
        cg.add(var.set_state(CONF_STATE_TEMPERATURE_TARGET, state))

    if CONF_STATE_ECO in config:
        state = state_hex_expression(config[CONF_STATE_ECO])
        cg.add(var.set_state(CONF_STATE_ECO, state))

    if CONF_STATE_ELECTRIC in config:
        state = state_hex_expression(config[CONF_STATE_ELECTRIC])
        cg.add(var.set_state(CONF_STATE_ELECTRIC, state))

    if CONF_STATE_PERFORMANCE in config:
        state = state_hex_expression(config[CONF_STATE_PERFORMANCE])
        cg.add(var.set_state(CONF_STATE_PERFORMANCE, state))

    if CONF_STATE_HIGH_DEMAND in config:
        state = state_hex_expression(config[CONF_STATE_HIGH_DEMAND])
        cg.add(var.set_state(CONF_STATE_HIGH_DEMAND, state))

    if CONF_STATE_HEAT_PUMP in config:
        state = state_hex_expression(config[CONF_STATE_HEAT_PUMP])
        cg.add(var.set_state(CONF_STATE_HEAT_PUMP, state))

    if CONF_STATE_GAS in config:
        state = state_hex_expression(config[CONF_STATE_GAS])
        cg.add(var.set_state(CONF_STATE_GAS, state))

    if CONF_STATE_AWAY_ON in config:
        state = state_hex_expression(config[CONF_STATE_AWAY_ON])
        cg.add(var.set_state(CONF_STATE_AWAY_ON, state))

    if CONF_STATE_AWAY_OFF in config:
        state = state_hex_expression(config[CONF_STATE_AWAY_OFF])
        cg.add(var.set_state(CONF_STATE_AWAY_OFF, state))

    if CONF_COMMAND_TEMPERATURE in config:
        command = await command_float_expression(config[CONF_COMMAND_TEMPERATURE])
        cg.add(var.set_command(CONF_COMMAND_TEMPERATURE, command))

    if CONF_COMMAND_ECO in config:
        command = await command_expression(config[CONF_COMMAND_ECO])
        cg.add(var.set_command(CONF_COMMAND_ECO, command))

    if CONF_COMMAND_ELECTRIC in config:
        command = await command_expression(config[CONF_COMMAND_ELECTRIC])
        cg.add(var.set_command(CONF_COMMAND_ELECTRIC, command))

    if CONF_COMMAND_PERFORMANCE in config:
        command = await command_expression(config[CONF_COMMAND_PERFORMANCE])
        cg.add(var.set_command(CONF_COMMAND_PERFORMANCE, command))

    if CONF_COMMAND_HIGH_DEMAND in config:
        command = await command_expression(config[CONF_COMMAND_HIGH_DEMAND])
        cg.add(var.set_command(CONF_COMMAND_HIGH_DEMAND, command))

    if CONF_COMMAND_HEAT_PUMP in config:
        command = await command_expression(config[CONF_COMMAND_HEAT_PUMP])
        cg.add(var.set_command(CONF_COMMAND_HEAT_PUMP, command))

    if CONF_COMMAND_GAS in config:
        command = await command_expression(config[CONF_COMMAND_GAS])
        cg.add(var.set_command(CONF_COMMAND_GAS, command))

    if CONF_COMMAND_AWAY_ON in config:
        command = await command_expression(config[CONF_COMMAND_AWAY_ON])
        cg.add(var.set_command(CONF_COMMAND_AWAY_ON, command))

    if CONF_COMMAND_AWAY_OFF in config:
        command = await command_expression(config[CONF_COMMAND_AWAY_OFF])
        cg.add(var.set_command(CONF_COMMAND_AWAY_OFF, command))
