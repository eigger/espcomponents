import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, uartex
from esphome.const import CONF_OUTPUT_ID, \
    CONF_STATE, CONF_COMMAND, CONF_UPDATE_INTERVAL
from .. import uartex_ns, cmd_t, uint8_ptr_const, uint16_const
from ..const import CONF_SPEED_CNT, CONF_STATE_SPEED, CONF_COMMAND_SPEED

DEPENDENCIES = ['uartex']
UARTExFan = uartex_ns.class_('UARTExFan', cg.Component)

CONFIG_SCHEMA = cv.All(fan.FAN_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(UARTExFan),
    cv.Optional(CONF_SPEED_CNT, default=3): cv.int_range(min=1, max=100),
    cv.Required(CONF_STATE_SPEED): cv.returning_lambda,
    cv.Required(CONF_COMMAND_SPEED): cv.returning_lambda,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    interval = config[CONF_UPDATE_INTERVAL]
    del config[CONF_UPDATE_INTERVAL]
    config[CONF_UPDATE_INTERVAL] = interval
    await cg.register_component(var, config)
    await fan.register_fan(var, config)
    await uartex.register_uartex_device(var, config)

    templ = await cg.templatable(config[CONF_COMMAND_SPEED], [(cg.float_.operator('const'), 'x')], cmd_t)
    cg.add(var.set_command_speed(templ))

    templ = await cg.templatable(config[CONF_STATE_SPEED], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
    cg.add(var.set_state_speed(templ))

    if CONF_SPEED_CNT in config:
        cg.add(var.set_speed_count(config[CONF_SPEED_CNT]))

