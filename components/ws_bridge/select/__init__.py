import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select, ws_bridge
from esphome.const import CONF_OPTIONS

from .. import ws_bridge_ns

DEPENDENCIES = ["ws_bridge"]
WsBridgeSelect = ws_bridge_ns.class_("WsBridgeSelect", select.Select, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    select.select_schema(WsBridgeSelect)
    .extend(
        {
            cv.Required(CONF_OPTIONS): cv.All(cv.ensure_list(cv.string_strict), cv.Length(min=1)),
        }
    )
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await select.new_select(config, options=config[CONF_OPTIONS])
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
