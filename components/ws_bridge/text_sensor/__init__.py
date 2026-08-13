import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_TEXT_SENSOR_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeTextSensor = ws_bridge_ns.class_(
    "WsBridgeTextSensor", text_sensor.TextSensor, cg.Component, ws_bridge.WsBridgeDevice
)

CONFIG_SCHEMA = (
    text_sensor.text_sensor_schema(WsBridgeTextSensor)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap an existing ESPHome text_sensor and expose it to Home Assistant.
            cv.Optional(CONF_TEXT_SENSOR_ID): cv.use_id(text_sensor.TextSensor),
        }
    )
)


async def to_code(config):
    var = await text_sensor.new_text_sensor(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_TEXT_SENSOR_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_TEXT_SENSOR_ID])))
