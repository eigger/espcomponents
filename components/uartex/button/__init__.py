import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, uartex
from esphome.const import CONF_ID
from .. import uartex_ns, UARTExDevice
from ..const import CONF_STATE, CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_OFF, CONF_COMMAND_UPDATE

DEPENDENCIES = ['uartex']
UARTExButton = uartex_ns.class_('UARTExButton', button.Button, UARTExDevice)

CONFIG_SCHEMA = button.button_schema(UARTExButton).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE): cv.invalid("UARTEx buttons do not support state!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx buttons do not support state on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx buttons do not support state off!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx buttons do not support command off!"),
    cv.Optional(CONF_COMMAND_UPDATE): cv.invalid("UARTEx buttons do not support commad state!")
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)
