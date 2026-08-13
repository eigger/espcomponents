import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_SWITCH_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeSwitch = ws_bridge_ns.class_("WsBridgeSwitch", switch.Switch, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    switch.switch_schema(WsBridgeSwitch)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap and drive an existing ESPHome switch. HA commands are
            # applied to it (not to this entity), and its state changes are
            # what get reported back — see WsBridgeSwitch::ws_bridge_handle_command.
            cv.Optional(CONF_SWITCH_ID): cv.use_id(switch.Switch),
        }
    )
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_SWITCH_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_SWITCH_ID])))
