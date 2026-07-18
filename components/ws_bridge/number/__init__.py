import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, ws_bridge
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP

from .. import ws_bridge_ns

DEPENDENCIES = ["ws_bridge"]
WsBridgeNumber = ws_bridge_ns.class_("WsBridgeNumber", number.Number, cg.Component, ws_bridge.WsBridgeDevice)


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(WsBridgeNumber)
    .extend(
        {
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP, default=1.0): cv.positive_float,
        }
    )
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max,
)


async def to_code(config):
    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
