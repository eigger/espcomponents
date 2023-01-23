import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, bluetoothex
from esphome.const import CONF_ID, CONF_INVERTED
from .. import bluetoothex_ns

DEPENDENCIES = ['bluetoothex']
BluetoothExSwitch = bluetoothex_ns.class_('BluetoothExSwitch', switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BluetoothExSwitch),
    cv.Optional(CONF_INVERTED): cv.invalid("BluetoothEx switches do not support inverted mode!")
}).extend(bluetoothex.BLUETOOTHEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield bluetoothex.register_bluetoothex_device(var, config)
