import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import event, ws_bridge
from esphome.const import CONF_EVENT_TYPES

from .. import ws_bridge_ns
from ..const import CONF_EVENT_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeEvent = ws_bridge_ns.class_("WsBridgeEvent", event.Event, cg.Component, ws_bridge.WsBridgeDevice)

# An empty Python list renders as a bare `{}`, which is ambiguous across
# Event::set_event_types()'s initializer_list / FixedVector / vector overloads
# (including the =delete'd std::string ones that exist to catch mistakes). Name
# the type so the vector overload is an exact match. The wrapper's real list is
# copied off the source in setup() — see WsBridgeEvent::setup().
_NO_EVENT_TYPES = cg.RawExpression("std::vector<const char *>{}")


def validate_event_types(config):
    if CONF_EVENT_TYPES not in config and CONF_EVENT_ID not in config:
        raise cv.Invalid("event_types is required unless event_id: is set")
    return config


CONFIG_SCHEMA = cv.All(
    event.event_schema(WsBridgeEvent)
    .extend(
        {
            cv.Optional(CONF_EVENT_TYPES): cv.All(cv.ensure_list(cv.string_strict), cv.Length(min=1)),
            # Wrap an existing ESPHome event. Its triggers are what get
            # reported, and its event_types are what get declared, so
            # event_types: may be omitted here.
            cv.Optional(CONF_EVENT_ID): cv.use_id(event.Event),
        }
    )
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_event_types,
)


async def to_code(config):
    var = await event.new_event(config, event_types=config.get(CONF_EVENT_TYPES, _NO_EVENT_TYPES))
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_EVENT_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_EVENT_ID])))
