import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_CLIMATE_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeClimate = ws_bridge_ns.class_("WsBridgeClimate", climate.Climate, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    climate.climate_schema(WsBridgeClimate)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap and drive an existing ESPHome climate. HA commands are
            # applied to it, and its state changes are what get reported back.
            cv.Optional(CONF_CLIMATE_ID): cv.use_id(climate.Climate),
        }
    )
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_CLIMATE_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_CLIMATE_ID])))
