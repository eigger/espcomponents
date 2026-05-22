import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, esp32_ble_tracker
from esphome.const import (
    CONF_ID, CONF_SERVICE_UUID, CONF_MAC_ADDRESS,
)
from .const import (
    CONF_BLE_ELM327_ID, CONF_INTERNAL_BLE_CLIENT_ID, CONF_RX_CHAR_UUID, CONF_TX_CHAR_UUID,
    CONF_INIT_COMMANDS, CONF_TX_DELAY, CONF_PID, CONF_MODE, CONF_FORMULA, CONF_PRESET,
)
from .presets import OBD_PRESETS

AUTO_LOAD = ["ble_client"]
CODEOWNERS = ["@eigger"]
MULTI_CONF = True

ble_elm327_ns = cg.esphome_ns.namespace("ble_elm327")
# Component is NOT a PollingComponent — per-device polling only
BleElm327Component = ble_elm327_ns.class_(
    "BleElm327Component", cg.Component, ble_client.BLEClientNode
)
# Device IS a PollingComponent — each sensor owns its update_interval
BleElm327Device = ble_elm327_ns.class_("BleElm327Device", cg.PollingComponent)


def _add_uuid(var, uuid_val, fn16, fn32, fn128):
    if len(uuid_val) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(getattr(var, fn16)(esp32_ble_tracker.as_hex(uuid_val)))
    elif len(uuid_val) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(getattr(var, fn32)(esp32_ble_tracker.as_hex(uuid_val)))
    elif len(uuid_val) == len(esp32_ble_tracker.bt_uuid128_format):
        cg.add(getattr(var, fn128)(esp32_ble_tracker.as_reversed_hex_array(uuid_val)))


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BleElm327Component),
        cv.GenerateID(CONF_INTERNAL_BLE_CLIENT_ID): cv.declare_id(ble_client.BLEClient),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_SERVICE_UUID, default="FFF0"): esp32_ble_tracker.bt_uuid,
        cv.Optional(CONF_RX_CHAR_UUID, default="FFF1"): esp32_ble_tracker.bt_uuid,
        cv.Optional(CONF_TX_CHAR_UUID, default="FFF2"): esp32_ble_tracker.bt_uuid,
        cv.Optional(CONF_INIT_COMMANDS, default=["ATZ", "ATE0", "ATL0", "ATS0", "ATSP0"]):
            cv.ensure_list(cv.string_strict),
        cv.Optional(CONF_TX_DELAY, default=50): cv.positive_int,           # ms
    }
).extend(esp32_ble_tracker.ESP_BLE_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Auto-create an internal BLEClient bound to mac_address — user needs no separate ble_client: block
    ble = cg.new_Pvariable(config[CONF_INTERNAL_BLE_CLIENT_ID])
    await cg.register_component(ble, {})
    await esp32_ble_tracker.register_ble_device(ble, config)
    cg.add(ble.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(ble.register_ble_node(var))

    _add_uuid(var, config[CONF_SERVICE_UUID],
              "set_service_uuid16", "set_service_uuid32", "set_service_uuid128")
    _add_uuid(var, config[CONF_RX_CHAR_UUID],
              "set_rx_char_uuid16", "set_rx_char_uuid32", "set_rx_char_uuid128")
    _add_uuid(var, config[CONF_TX_CHAR_UUID],
              "set_tx_char_uuid16", "set_tx_char_uuid32", "set_tx_char_uuid128")

    for cmd in config[CONF_INIT_COMMANDS]:
        cg.add(var.add_init_command(cmd))

    cg.add(var.set_tx_delay(config[CONF_TX_DELAY]))


# ── Shared device schema (used by every sub-platform) ────────────────────────

# NOTE: pid is Optional here; the platform-level CONFIG_SCHEMA must wrap with
# _validate_preset (cv.All) which ensures either preset or pid is present.
BLE_ELM327_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLE_ELM327_ID): cv.use_id(BleElm327Component),
        cv.Optional(CONF_PRESET): cv.one_of(*OBD_PRESETS),
        cv.Optional(CONF_PID): cv.string_strict,
        cv.Optional(CONF_MODE, default="01"): cv.string_strict,
        cv.Optional(CONF_FORMULA): cv.returning_lambda,
    }
).extend(cv.polling_component_schema("60s"))


class LambdaString(str):
    def __getattr__(self, name):
        return None


def inject_preset(config):
    """Inject preset values as defaults before schema validation.

    Call this as the first step of cv.All() in each platform's CONFIG_SCHEMA.
    Sensor-level fields (unit, device_class, state_class, accuracy_decimals)
    are also injected here so sensor.sensor_schema() picks them up.
    """
    if CONF_PRESET not in config:
        if CONF_PID not in config:
            raise cv.Invalid("Either 'preset' or 'pid' must be specified")
        return config

    p = OBD_PRESETS.get(config[CONF_PRESET])
    if p is None:
        raise cv.Invalid(
            f"Unknown preset '{config[CONF_PRESET]}'. "
            f"Valid presets: {', '.join(OBD_PRESETS)}"
        )
    for key, val in p.items():
        if key == CONF_FORMULA and isinstance(val, str) and not hasattr(val, 'esp_range'):
            val = LambdaString(val)
        config.setdefault(key, val)
    return config


async def register_ble_elm327_device(var, config):
    paren = await cg.get_variable(config[CONF_BLE_ELM327_ID])
    cg.add(paren.add_device(var))
    cg.add(var.set_pid(config[CONF_PID]))
    cg.add(var.set_mode(config[CONF_MODE]))

    if CONF_FORMULA in config:
        formula_ = await cg.process_lambda(
            config[CONF_FORMULA],
            [(cg.uint8, "a"), (cg.uint8, "b"), (cg.uint8, "c"), (cg.uint8, "d")],
            return_type=cg.float_,
        )
        cg.add(var.set_formula(formula_))
