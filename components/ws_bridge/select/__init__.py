import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select, ws_bridge
from esphome.const import CONF_OPTIONS

from .. import ws_bridge_ns
from ..const import CONF_SELECT_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeSelect = ws_bridge_ns.class_("WsBridgeSelect", select.Select, cg.Component, ws_bridge.WsBridgeDevice)


def validate_select_options(config):
    if CONF_OPTIONS not in config and CONF_SELECT_ID not in config:
        raise cv.Invalid("options is required unless select_id: is set")
    return config


CONFIG_SCHEMA = cv.All(
    select.select_schema(WsBridgeSelect)
    .extend(
        {
            cv.Optional(CONF_OPTIONS): cv.All(cv.ensure_list(cv.string_strict), cv.Length(min=1)),
            # Wrap and drive an existing ESPHome select. HA commands are
            # applied to it (not to this entity), and its state changes are
            # what get reported back.
            cv.Optional(CONF_SELECT_ID): cv.use_id(select.Select),
        }
    )
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_select_options,
)


async def to_code(config):
    # An omitted `options:` (only valid when wrapping) leaves this entity's
    # own SelectTraits empty, which ws_declare_select reads as "inherit the
    # wrapped select's options" — see ws_bridge_domains.h.
    var = await select.new_select(config, options=config.get(CONF_OPTIONS, []))
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_SELECT_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_SELECT_ID])))
