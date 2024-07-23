import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, uartex
from esphome.const import CONF_ID, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP
from esphome.components.uartex import uartex_ns
from esphome.components.uartex.const import CONF_COMMAND_OFF, CONF_COMMAND_ON, CONF_STATE_OFF, CONF_STATE_ON

DEPENDENCIES = ['uartex']
BotemCSM505 = uartex_ns.class_('BotemCSM505', number.Number, cg.Component)

CONFIG_SCHEMA = cv.All(number.NUMBER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BotemCSM505),
    cv.Required(CONF_MIN_VALUE): cv.float_,
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Required(CONF_STEP): cv.float_,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("BotemCSM505 do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("BotemCSM505 do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("BotemCSM505 do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("BotemCSM505 do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(            
        var,
        config,
        min_value = config[CONF_MIN_VALUE],
        max_value = config[CONF_MAX_VALUE],
        step = config[CONF_STEP],)
    await uartex.register_uartex_device(var, config)
