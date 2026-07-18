import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, ws_bridge

from .. import ws_bridge_ns

DEPENDENCIES = ["ws_bridge"]
WsBridgeSwitch = ws_bridge_ns.class_("WsBridgeSwitch", switch.Switch, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    switch.switch_schema(WsBridgeSwitch)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
