import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, esp32_ble_tracker
from esphome.const import CONF_ID, CONF_SERVICE_UUID, CONF_MAC_ADDRESS

DEPENDENCIES = ["ble_client"]
CODEOWNERS = ["@eigger"]
MULTI_CONF = True

CONF_BLE_ELM327_ID = "ble_elm327_id"
CONF_INTERNAL_BLE_CLIENT_ID = "ble_elm327_internal_ble_client_id"
CONF_RX_CHAR_UUID = "rx_char_uuid"
CONF_TX_CHAR_UUID = "tx_char_uuid"
CONF_INIT_COMMANDS = "init_commands"
CONF_TX_DELAY = "tx_delay"
CONF_PID = "pid"
CONF_MODE = "mode"
CONF_RESPONSE_SIZE = "response_size"
CONF_FORMULA = "formula"

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

BLE_ELM327_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLE_ELM327_ID): cv.use_id(BleElm327Component),
        cv.Required(CONF_PID): cv.string_strict,
        cv.Optional(CONF_MODE, default="01"): cv.string_strict,
        cv.Optional(CONF_RESPONSE_SIZE, default=2): cv.positive_int,
        cv.Optional(CONF_FORMULA): cv.returning_lambda,
    }
).extend(cv.polling_component_schema("60s"))


async def register_ble_elm327_device(var, config):
    paren = await cg.get_variable(config[CONF_BLE_ELM327_ID])
    cg.add(paren.add_device(var))
    cg.add(var.set_pid(config[CONF_PID]))
    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_response_size(config[CONF_RESPONSE_SIZE]))

    if CONF_FORMULA in config:
        formula_ = await cg.process_lambda(
            config[CONF_FORMULA],
            [(cg.uint8, "a"), (cg.uint8, "b"), (cg.uint8, "c"), (cg.uint8, "d")],
            return_type=cg.float_,
        )
        cg.add(var.set_formula(formula_))
