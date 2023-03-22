import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uartex
from esphome.const import CONF_ID, CONF_INVERTED
from .. import uartex_ns, _uartex_declare_type
from ..const import CONF_UARTEX_ID

DEPENDENCIES = ['uartex']
UARTExSwitch = uartex_ns.class_('UARTExSwitch', switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExSwitch),
    cv.GenerateID(CONF_UARTEX_ID): _uartex_declare_type,
    cv.Optional(CONF_INVERTED): cv.invalid("UARTEx switches do not support inverted mode!")
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield uartex.register_uartex_device(var, config)
