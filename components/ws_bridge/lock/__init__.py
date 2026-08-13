import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import lock, ws_bridge

from .. import ws_bridge_ns
from ..const import CONF_CODE_FORMAT, CONF_LOCK_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeLock = ws_bridge_ns.class_("WsBridgeLock", lock.Lock, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    lock.lock_schema(WsBridgeLock)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Regex HA validates the typed code against before sending the
            # command. Declare-only: ESPHome's lock domain cannot receive a
            # code, so this is a UI confirmation prompt, not authentication.
            cv.Optional(CONF_CODE_FORMAT): cv.string_strict,
            # Wrap and drive an existing ESPHome lock. HA commands are applied
            # to it, and its state changes are what get reported back.
            cv.Optional(CONF_LOCK_ID): cv.use_id(lock.Lock),
        }
    )
)


async def to_code(config):
    var = await lock.new_lock(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_CODE_FORMAT in config:
        cg.add(var.set_code_format(config[CONF_CODE_FORMAT]))
    if CONF_LOCK_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_LOCK_ID])))
