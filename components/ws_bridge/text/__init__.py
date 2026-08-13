import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text, ws_bridge
from esphome.const import CONF_MAX_LENGTH, CONF_MIN_LENGTH, CONF_PATTERN

from .. import ws_bridge_ns
from ..const import CONF_TEXT_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeText = ws_bridge_ns.class_("WsBridgeText", text.Text, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    # mode: defaults to TEXT here — ESPHome's own text_schema() makes it
    # required, which would be noise on a bridge entity.
    text.text_schema(WsBridgeText, mode="TEXT")
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # String *length* bounds, not a value range (unlike number's
            # min_value/max_value). ESPHome's own defaults.
            cv.Optional(CONF_MIN_LENGTH, default=0): cv.int_range(min=0, max=255),
            cv.Optional(CONF_MAX_LENGTH, default=255): cv.int_range(min=0, max=255),
            cv.Optional(CONF_PATTERN): cv.string,
            # Wrap and drive an existing ESPHome text. HA commands are applied
            # to it, and its state changes are what get reported back. Its
            # length limits, pattern and mode are what get declared — see
            # ws_declare_text in ws_bridge_domains.h.
            cv.Optional(CONF_TEXT_ID): cv.use_id(text.Text),
        }
    )
)


async def to_code(config):
    var = await text.new_text(
        config,
        min_length=config[CONF_MIN_LENGTH],
        max_length=config[CONF_MAX_LENGTH],
        pattern=config.get(CONF_PATTERN),
    )
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_TEXT_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_TEXT_ID])))
