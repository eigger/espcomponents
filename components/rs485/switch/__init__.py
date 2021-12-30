import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, rs485
from esphome.const import CONF_ID, CONF_INVERTED
from .. import rs485_ns

DEPENDENCIES = ['rs485']
RS485Switch = rs485_ns.class_('RS485Switch', switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(RS485Switch),
    cv.Optional(CONF_INVERTED): cv.invalid("RS485 switches do not support inverted mode!")
}).extend(rs485.RS485_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield rs485.register_rs485_device(var, config)
