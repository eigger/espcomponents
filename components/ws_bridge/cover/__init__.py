import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_COVER_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeCover = ws_bridge_ns.class_("WsBridgeCover", cover.Cover, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    cover.cover_schema(WsBridgeCover)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap and drive an existing ESPHome cover. HA commands are
            # applied to it, and its state changes are what get reported back.
            cv.Optional(CONF_COVER_ID): cv.use_id(cover.Cover),
        }
    )
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_COVER_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_COVER_ID])))
