import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, wallpad
from esphome.const import CONF_ID, CONF_INVERTED
from .. import wallpad_ns

DEPENDENCIES = ['wallpad']
WallPadButton = wallpad_ns.class_('WallPadButton', button.Button, cg.Component)

CONFIG_SCHEMA = button.BUTTON_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(WallPadButton),
    cv.Optional(CONF_INVERTED): cv.invalid("WallPad buttons do not support inverted mode!")
}).extend(wallpad.WallPad_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield button.register_button(var, config)
    yield wallpad.register_wallpad_device(var, config)
