import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, ws_bridge
from esphome.const import CONF_ID, CONF_NAME

from .. import ws_bridge_ns
from ..const import CONF_LIGHT_ID

DEPENDENCIES = ["ws_bridge"]

# Wraps an existing ESPHome LightState and exposes it to Home Assistant.
# light_id: is required — inheriting LightOutput would create a second state.
WsBridgeLight = ws_bridge_ns.class_("WsBridgeLight", cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WsBridgeLight),
            cv.Required(CONF_LIGHT_ID): cv.use_id(light.LightState),
            cv.Optional(CONF_NAME): cv.string_strict,
        }
    )
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    cg.add(var.set_light(await cg.get_variable(config[CONF_LIGHT_ID])))
    if CONF_NAME in config:
        cg.add(var.set_name(config[CONF_NAME]))
