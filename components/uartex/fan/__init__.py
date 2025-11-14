import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, uartex
from esphome.const import CONF_ID, CONF_PRESET_MODES
from .. import uartex_ns, UARTExDevice, \
    state_num_schema, state_num_expression, state_string_expression, \
    command_hex_schema, command_float_expression, command_string_expression, validate_version
from ..const import CONF_SPEED_CNT, CONF_STATE_SPEED, CONF_COMMAND_SPEED, CONF_STATE_PRESET, CONF_COMMAND_PRESET

DEPENDENCIES = ['uartex']
UARTExFan = uartex_ns.class_('UARTExFan', fan.Fan, UARTExDevice)

CONFIG_SCHEMA = cv.All(fan.fan_schema(UARTExFan).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_SPEED_CNT, default=3): cv.int_range(min=1, max=100),
    cv.Optional(CONF_PRESET_MODES): fan.validate_preset_modes,
    cv.Required(CONF_STATE_SPEED): cv.templatable(state_num_schema),
    cv.Required(CONF_COMMAND_SPEED): cv.templatable(command_hex_schema),
    cv.Optional(CONF_STATE_PRESET): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_PRESET): cv.templatable(command_hex_schema),
}).extend(cv.COMPONENT_SCHEMA), validate_version)

async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_SPEED in config:
        state = await state_num_expression(config[CONF_STATE_SPEED])
        cg.add(var.set_state(CONF_STATE_SPEED, state))

    if CONF_COMMAND_SPEED in config:
        command = await command_float_expression(config[CONF_COMMAND_SPEED])
        cg.add(var.set_command(CONF_COMMAND_SPEED, command))

    if CONF_SPEED_CNT in config:
        cg.add(var.set_speed_count(config[CONF_SPEED_CNT]))

    if CONF_PRESET_MODES in config:
        cg.add(var.set_preset_modes(config[CONF_PRESET_MODES]))

    if CONF_STATE_PRESET in config:
        state = await state_string_expression(config[CONF_STATE_PRESET])
        cg.add(var.set_state(CONF_STATE_PRESET, state))
        
    if CONF_COMMAND_PRESET in config:
        command = await command_string_expression(config[CONF_COMMAND_PRESET])
        cg.add(var.set_command(CONF_COMMAND_PRESET, command))

