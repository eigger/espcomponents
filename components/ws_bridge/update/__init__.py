import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import update, ws_bridge
from esphome.const import CONF_ID, CONF_NAME

from .. import ws_bridge_ns
from ..const import CONF_UPDATE_ID

DEPENDENCIES = ["ws_bridge"]

# Wraps an existing ESPHome UpdateEntity (typically `update: platform: http_request`
# from the esphome_ota add-on) and exposes it to Home Assistant over ws_bridge.
# The http_request platform must be declared separately so its C++ is compiled;
# this platform only bridges state/commands.
WsBridgeUpdate = ws_bridge_ns.class_("WsBridgeUpdate", cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WsBridgeUpdate),
            cv.Required(CONF_UPDATE_ID): cv.use_id(update.UpdateEntity),
            # HA-facing name. Do not rely on the wrapped http_request entity —
            # an id-only update gets name=id and internal: true.
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
    cg.add(var.set_update(await cg.get_variable(config[CONF_UPDATE_ID])))
    if CONF_NAME in config:
        cg.add(var.set_name(config[CONF_NAME]))
