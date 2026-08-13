import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import fan, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_FAN_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeFan = ws_bridge_ns.class_("WsBridgeFan", fan.Fan, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    fan.fan_schema(WsBridgeFan)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap and drive an existing ESPHome fan. HA commands are applied
            # to it, and its state changes are what get reported back.
            cv.Optional(CONF_FAN_ID): cv.use_id(fan.Fan),
        }
    )
)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_FAN_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_FAN_ID])))
