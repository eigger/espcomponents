import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import valve, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_VALVE_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeValve = ws_bridge_ns.class_("WsBridgeValve", valve.Valve, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    valve.valve_schema(WsBridgeValve)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap and drive an existing ESPHome valve. HA commands are applied
            # to it, and its state changes are what get reported back.
            cv.Optional(CONF_VALVE_ID): cv.use_id(valve.Valve),
        }
    )
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_VALVE_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_VALVE_ID])))
