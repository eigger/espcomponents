import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, ws_bridge

from .. import ws_bridge_ns

DEPENDENCIES = ["ws_bridge"]
WsBridgeBinarySensor = ws_bridge_ns.class_(
    "WsBridgeBinarySensor", binary_sensor.BinarySensor, cg.Component, ws_bridge.WsBridgeDevice
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(WsBridgeBinarySensor)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
