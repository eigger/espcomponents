import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, ws_bridge
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP

from .. import ws_bridge_ns
from ..const import CONF_NUMBER_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeNumber = ws_bridge_ns.class_("WsBridgeNumber", number.Number, cg.Component, ws_bridge.WsBridgeDevice)


def validate_number_range(config):
    has_min = CONF_MIN_VALUE in config
    has_max = CONF_MAX_VALUE in config
    if has_min != has_max:
        raise cv.Invalid("min_value and max_value must be set together")
    if not has_min:
        # No range of its own: only valid when wrapping, since the range then
        # comes from number_id's own min/max at declare time (see
        # ws_bridge_domains.h's ws_declare_number).
        if CONF_NUMBER_ID not in config:
            raise cv.Invalid("min_value and max_value are required unless number_id: is set")
        return config
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(WsBridgeNumber)
    .extend(
        {
            cv.Optional(CONF_MIN_VALUE): cv.float_,
            cv.Optional(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP): cv.positive_float,
            # Wrap and drive an existing ESPHome number. HA commands are
            # applied to it (not to this entity), and its state changes are
            # what get reported back.
            cv.Optional(CONF_NUMBER_ID): cv.use_id(number.Number),
        }
    )
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_number_range,
)


async def to_code(config):
    wrapping = CONF_NUMBER_ID in config
    var = await number.new_number(
        config,
        # NaN means "inherit this field from number_id" (see ws_declare_number),
        # so the 1.0 step default only applies when there is nothing to inherit
        # from — otherwise overriding just the range would also force step 1.0.
        min_value=config.get(CONF_MIN_VALUE, float("nan")),
        max_value=config.get(CONF_MAX_VALUE, float("nan")),
        step=config.get(CONF_STEP, float("nan") if wrapping else 1.0),
    )
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_NUMBER_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_NUMBER_ID])))
