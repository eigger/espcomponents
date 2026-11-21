import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, uartex
from esphome.const import CONF_ID
from .. import uartex_ns, UARTExDevice
from ..const import CONF_COMMAND_ON, CONF_COMMAND_OFF

DEPENDENCIES = ['uartex']
UARTExBinarySensor = uartex_ns.class_('UARTExBinarySensor', binary_sensor.BinarySensor, UARTExDevice)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(UARTExBinarySensor).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Binary Sensor do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Binary Sensor do not support command_off!")
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)
