import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, uartex
from esphome.const import CONF_ID, CONF_INVERTED
from .. import uartex_ns
from ..const import CONF_STATE, CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_OFF, CONF_COMMAND_UPDATE

DEPENDENCIES = ['uartex']
UARTExButton = uartex_ns.class_('UARTExButton', button.Button, cg.Component)

CONFIG_SCHEMA = button.BUTTON_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExButton),
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_INVERTED): cv.invalid("UARTEx buttons do not support inverted mode!"),
    cv.Optional(CONF_STATE): cv.invalid("UARTEx buttons do not support filter!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx buttons do not support state on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx buttons do not support state off!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx buttons do not support command off!"),
    cv.Optional(CONF_COMMAND_UPDATE): cv.invalid("UARTEx buttons do not support commad state!")
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
    await uartex.register_uartex_device(var, config)
