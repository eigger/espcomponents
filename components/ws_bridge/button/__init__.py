import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, ws_bridge

from .. import ws_bridge_ns

DEPENDENCIES = ["ws_bridge"]
WsBridgeButton = ws_bridge_ns.class_("WsBridgeButton", button.Button, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    button.button_schema(WsBridgeButton)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
