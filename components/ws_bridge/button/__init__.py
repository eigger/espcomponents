import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_BUTTON_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeButton = ws_bridge_ns.class_("WsBridgeButton", button.Button, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    button.button_schema(WsBridgeButton)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        cv.Schema(
            {
                # Wrap an existing ESPHome button (e.g. ota_flash_button from
                # ota_server/flash_button.yaml) and expose it to Home Assistant.
                cv.Optional(CONF_BUTTON_ID): cv.use_id(button.Button),
            }
        )
    )
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_BUTTON_ID in config:
        cg.add(var.set_button(await cg.get_variable(config[CONF_BUTTON_ID])))
