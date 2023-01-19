import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import binary_sensor, bluetoothex
from esphome.const import CONF_ID
from .. import bluetoothex_ns
from ..const import CONF_COMMAND_ON, CONF_COMMAND_OFF

DEPENDENCIES = ['bluetoothex']
BluetoothExBinarySensor = bluetoothex_ns.class_('BluetoothExBinarySensor', binary_sensor.BinarySensor,
                                      cg.Component)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BluetoothExBinarySensor),
}).extend(bluetoothex.BLUETOOTHEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("BluetoothEx Binary Sensor do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("BluetoothEx Binary Sensor do not support command_off!")
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield binary_sensor.register_binary_sensor(var, config)
    yield bluetoothex.register_bluetoothex_device(var, config)
