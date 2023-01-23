import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, bluetoothex
from esphome.const import CONF_ID, CONF_INVERTED
from ..const import CONF_SUB_FILTER, CONF_FILTER, CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_OFF, CONF_COMMAND_UPDATE
from .. import bluetoothex_ns

DEPENDENCIES = ['bluetoothex']
BluetoothExButton = bluetoothex_ns.class_('BluetoothExButton', button.Button, cg.Component)

CONFIG_SCHEMA = button.BUTTON_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BluetoothExButton),
}).extend(bluetoothex.BLUETOOTHEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_INVERTED): cv.invalid("BluetoothEx buttons do not support inverted mode!"),
    cv.Optional(CONF_FILTER): cv.invalid("BluetoothEx buttons do not support filter!"),
    cv.Optional(CONF_SUB_FILTER): cv.invalid("BluetoothEx buttons do not support sub device!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("BluetoothEx buttons do not support state on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("BluetoothEx buttons do not support state off!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("BluetoothEx buttons do not support command off!"),
    cv.Optional(CONF_COMMAND_UPDATE): cv.invalid("BluetoothEx buttons do not support commad state!")
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield button.register_button(var, config)
    yield bluetoothex.register_bluetoothex_device(var, config)
