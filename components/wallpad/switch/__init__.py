import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, wallpad
from esphome.const import CONF_ID, CONF_INVERTED
from .. import wallpad_ns

DEPENDENCIES = ['wallpad']
WallPadSwitch = wallpad_ns.class_('WallPadSwitch', switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(WallPadSwitch),
    cv.Optional(CONF_INVERTED): cv.invalid("WallPad switches do not support inverted mode!")
}).extend(wallpad.WallPad_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield wallpad.register_wallpad_device(var, config)
