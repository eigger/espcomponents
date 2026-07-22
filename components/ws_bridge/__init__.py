import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import esp32
from esphome.const import CONF_ID, CONF_PORT, CONF_NAME, CONF_TRIGGER_ID
from esphome.core import CORE

from .const import (
    CONF_WS_BRIDGE_ID,
    CONF_HOST,
    CONF_SSL,
    CONF_TOKEN,
    CONF_GATEWAY_ID,
    CONF_KEEP_LAST_STATE_ON_DISCONNECT,
    CONF_UNIQUE_ID,
    CONF_WS_DEVICE_ID,
    CONF_WS_DEVICE_NAME,
    CONF_ON_CONNECTED,
    CONF_ON_DISCONNECTED,
    CONF_PING_INTERVAL,
    CONF_PONG_TIMEOUT,
    CONF_RECONNECT_TIMEOUT,
    CONF_REANNOUNCE_INTERVAL,
)

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["json"]

ESP_WEBSOCKET_CLIENT_VERSION = "1.7.0"

ws_bridge_ns = cg.esphome_ns.namespace("ws_bridge")
WsBridgeComponent = ws_bridge_ns.class_("WsBridgeComponent", cg.Component)
WsBridgeDevice = ws_bridge_ns.class_("WsBridgeDevice")

ConnectedTrigger = ws_bridge_ns.class_("ConnectedTrigger", automation.Trigger.template())
DisconnectedTrigger = ws_bridge_ns.class_("DisconnectedTrigger", automation.Trigger.template())


def _validate_esp_idf(config):
    if CORE.target_framework != "esp-idf":
        raise cv.Invalid("ws_bridge requires the ESP-IDF framework (esp32: framework: type: esp-idf)")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WsBridgeComponent),
            cv.Required(CONF_HOST): cv.string_strict,
            cv.Optional(CONF_PORT, default=8123): cv.port,
            cv.Optional(CONF_SSL, default=True): cv.boolean,
            cv.Required(CONF_TOKEN): cv.string_strict,
            cv.Optional(CONF_GATEWAY_ID, default=lambda: CORE.name): cv.string_strict,
            cv.Optional(CONF_NAME, default=lambda: CORE.friendly_name or CORE.name): cv.string_strict,
            cv.Optional(CONF_KEEP_LAST_STATE_ON_DISCONNECT, default=False): cv.boolean,
            # See ws_bridge.cpp's check_liveness_() for what these govern: an
            # app-level ping/pong that detects a peer that dropped without a
            # clean WS close, and a backstop that forces a fresh connection
            # attempt if we've simply been disconnected too long (e.g. HA
            # itself restarting) for esp_websocket_client's own auto-reconnect
            # to have recovered on its own.
            cv.Optional(CONF_PING_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PONG_TIMEOUT, default="15s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RECONNECT_TIMEOUT, default="2min"): cv.positive_time_period_milliseconds,
            # Periodically resends ws_bridge/connect + entity declarations even
            # while nominally connected. Needed because the transport (and
            # HA's generic websocket_api ping/pong) can stay alive while the
            # ws_bridge integration on the HA side loses track of this specific
            # gateway (e.g. its config entry reloaded independently of the raw
            # connection) — that's invisible to ping/pong since HA core answers
            # pings regardless of our integration's state, so state pushes
            # would otherwise be silently dropped forever with no disconnect
            # ever observed.
            cv.Optional(CONF_REANNOUNCE_INTERVAL, default="5min"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_CONNECTED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ConnectedTrigger)}
            ),
            cv.Optional(CONF_ON_DISCONNECTED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DisconnectedTrigger)}
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_esp_idf,
)

# Shared schema every ws_bridge platform (sensor/binary_sensor/switch/number/
# select/button) must extend, mirroring uartex's UARTEX_DEVICE_SCHEMA pattern.
WS_BRIDGE_DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_WS_BRIDGE_ID): cv.use_id(WsBridgeComponent),
        cv.Required(CONF_UNIQUE_ID): cv.string_strict,
        cv.Optional(CONF_WS_DEVICE_ID): cv.string_strict,
        cv.Optional(CONF_WS_DEVICE_NAME): cv.string_strict,
    }
)


async def register_ws_bridge_device(var, config):
    parent = await cg.get_variable(config[CONF_WS_BRIDGE_ID])
    cg.add(var.set_ws_bridge_parent(parent))
    cg.add(var.set_unique_id(config[CONF_UNIQUE_ID]))
    if CONF_WS_DEVICE_ID in config:
        cg.add(var.set_device_id(config[CONF_WS_DEVICE_ID]))
    if CONF_WS_DEVICE_NAME in config:
        cg.add(var.set_device_name(config[CONF_WS_DEVICE_NAME]))
    cg.add(parent.register_device(var))


async def to_code(config):
    esp32.add_idf_component(name="espressif/esp_websocket_client", ref=ESP_WEBSOCKET_CLIENT_VERSION)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_host(config[CONF_HOST]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_ssl(config[CONF_SSL]))
    cg.add(var.set_token(config[CONF_TOKEN]))
    cg.add(var.set_gateway_id(config[CONF_GATEWAY_ID]))
    cg.add(var.set_gateway_name(config[CONF_NAME]))
    cg.add(var.set_keep_last_state_on_disconnect(config[CONF_KEEP_LAST_STATE_ON_DISCONNECT]))
    cg.add(var.set_ping_interval(config[CONF_PING_INTERVAL].total_milliseconds))
    cg.add(var.set_pong_timeout(config[CONF_PONG_TIMEOUT].total_milliseconds))
    cg.add(var.set_reconnect_timeout(config[CONF_RECONNECT_TIMEOUT].total_milliseconds))
    cg.add(var.set_reannounce_interval(config[CONF_REANNOUNCE_INTERVAL].total_milliseconds))

    for conf in config.get(CONF_ON_CONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISCONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
