import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_BINARY_SENSOR_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeBinarySensor = ws_bridge_ns.class_(
    "WsBridgeBinarySensor", binary_sensor.BinarySensor, cg.Component, ws_bridge.WsBridgeDevice
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(WsBridgeBinarySensor)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap an existing ESPHome binary_sensor and expose it to Home
            # Assistant. device_class is inherited from it unless also set here.
            cv.Optional(CONF_BINARY_SENSOR_ID): cv.use_id(binary_sensor.BinarySensor),
        }
    )
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_BINARY_SENSOR_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_BINARY_SENSOR_ID])))
