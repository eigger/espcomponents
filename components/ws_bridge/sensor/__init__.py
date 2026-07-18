import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ws_bridge

from .. import ws_bridge_ns

DEPENDENCIES = ["ws_bridge"]
WsBridgeSensor = ws_bridge_ns.class_("WsBridgeSensor", sensor.Sensor, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    sensor.sensor_schema(WsBridgeSensor)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
