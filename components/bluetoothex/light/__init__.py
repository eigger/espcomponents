import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, bluetoothex
from esphome.const import CONF_ID, CONF_NAME, CONF_OUTPUT_ID, CONF_UPDATE_INTERVAL
from .. import bluetoothex_ns

DEPENDENCIES = ['bluetoothex']
BluetoothExLightOutput = bluetoothex_ns.class_(
    'BluetoothExLightOutput', light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BluetoothExLightOutput),
}).extend(bluetoothex.BLUETOOTHEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    light_var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], var)
    cg.add(var.set_light(light_var))

    cg.add(cg.App.register_light(light_var))
    yield cg.register_component(var, config)
    del config[CONF_UPDATE_INTERVAL]
    yield cg.register_component(light_var, config)
    yield light.setup_light_core_(light_var, var, config)

    yield bluetoothex.register_bluetoothex_device(var, config)
