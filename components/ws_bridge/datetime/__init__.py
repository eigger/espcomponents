import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import datetime, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_DATETIME_ID

DEPENDENCIES = ["ws_bridge"]

# HA's date / time / datetime are three separate platforms, but ESPHome keeps
# all three in the single `datetime:` domain selected by `type:`. There is
# deliberately no `time/` directory here — ESPHome's `time:` component is a
# clock source (SNTP and friends), not an entity domain.
WsBridgeDate = ws_bridge_ns.class_("WsBridgeDate", datetime.DateEntity, cg.Component, ws_bridge.WsBridgeDevice)
WsBridgeTime = ws_bridge_ns.class_("WsBridgeTime", datetime.TimeEntity, cg.Component, ws_bridge.WsBridgeDevice)
WsBridgeDateTime = ws_bridge_ns.class_(
    "WsBridgeDateTime", datetime.DateTimeEntity, cg.Component, ws_bridge.WsBridgeDevice
)

_COMMON = ws_bridge.WS_BRIDGE_DEVICE_SCHEMA.extend(cv.COMPONENT_SCHEMA)


def _schema(base, source_type):
    # `datetime_id:` is typed per branch, so wrapping a TimeEntity from a
    # `type: DATE` entity is a config error rather than a compile error.
    return base.extend(_COMMON).extend({cv.Optional(CONF_DATETIME_ID): cv.use_id(source_type)})


CONFIG_SCHEMA = cv.typed_schema(
    {
        "DATE": _schema(datetime.date_schema(WsBridgeDate), datetime.DateEntity),
        "TIME": _schema(datetime.time_schema(WsBridgeTime), datetime.TimeEntity),
        "DATETIME": _schema(datetime.datetime_schema(WsBridgeDateTime), datetime.DateTimeEntity),
    },
    upper=True,
)


async def to_code(config):
    var = await datetime.new_datetime(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_DATETIME_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_DATETIME_ID])))
