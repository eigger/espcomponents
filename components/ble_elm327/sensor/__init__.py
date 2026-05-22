import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID
from .. import (
    ble_elm327_ns,
    BleElm327Device,
    BLE_ELM327_DEVICE_SCHEMA,
    register_ble_elm327_device,
)

DEPENDENCIES = ["ble_elm327"]

BleElm327Sensor = ble_elm327_ns.class_("BleElm327Sensor", sensor.Sensor, BleElm327Device)

# BLE_ELM327_DEVICE_SCHEMA already extends polling_component_schema("60s")
CONFIG_SCHEMA = sensor.sensor_schema(BleElm327Sensor).extend(BLE_ELM327_DEVICE_SCHEMA)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)   # sets update_interval on the PollingComponent
    await register_ble_elm327_device(var, config)
