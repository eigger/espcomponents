import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import (
    esp32,
    sensor,
    binary_sensor,
    text_sensor,
    switch,
    number,
    select,
    button,
    update,
)
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_NAME,
    CONF_TRIGGER_ID,
    CONF_ICON,
    CONF_LATITUDE,
    CONF_LONGITUDE,
)
from esphome.core import CORE

from .const import (
    CONF_WS_BRIDGE_ID,
    CONF_HOST,
    CONF_SSL,
    CONF_TOKEN,
    CONF_GATEWAY_ID,
    CONF_KEEP_LAST_STATE_ON_DISCONNECT,
    CONF_SYNC_ENTITIES,
    CONF_UNIQUE_ID,
    CONF_WS_DEVICE_ID,
    CONF_WS_DEVICE_NAME,
    CONF_ON_CONNECTED,
    CONF_ON_DISCONNECTED,
    CONF_ON_DECLARE,
    CONF_TRACKERS,
    CONF_GPS_ACCURACY,
    CONF_PING_INTERVAL,
    CONF_PONG_TIMEOUT,
    CONF_RECONNECT_TIMEOUT,
    CONF_REANNOUNCE_INTERVAL,
    CONF_ENTITIES,
    CONF_SOURCE_ID,
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
DeclareTrigger = ws_bridge_ns.class_("DeclareTrigger", automation.Trigger.template())

# device_tracker (GPS) entities. ESPHome has no device_tracker: domain to hang
# a `platform: ws_bridge` off, so these are configured inline under the hub —
# see README.md's "GPS device trackers" section.
WsBridgeTracker = ws_bridge_ns.class_("WsBridgeTracker", cg.PollingComponent, WsBridgeDevice)

TRACKER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WsBridgeTracker),
        cv.Required(CONF_UNIQUE_ID): cv.string_strict,
        cv.Optional(CONF_WS_DEVICE_ID): cv.string_strict,
        cv.Optional(CONF_WS_DEVICE_NAME): cv.string_strict,
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_ICON): cv.icon,
        cv.Required(CONF_LATITUDE): cv.templatable(cv.float_),
        cv.Required(CONF_LONGITUDE): cv.templatable(cv.float_),
        cv.Optional(CONF_GPS_ACCURACY): cv.templatable(cv.float_),
    }
).extend(cv.polling_component_schema("60s"))

# Hub-side `entities:` list — exposes an *existing* ESPHome entity (given by
# `source_id:`) to Home Assistant without a parallel `platform: ws_bridge`
# entity for it. One concrete *Ref class per domain, all sharing the same
# WsBridgeEntityRefBase — see ws_bridge_entity_ref.h. The domain is not
# something YAML declares; it's resolved from source_id's own registered type
# in to_code_entity_ref() below, since ESPHome doesn't know which platform an
# `id:` belongs to until that platform's own to_code has run.
WsBridgeEntityRefBase = ws_bridge_ns.class_("WsBridgeEntityRefBase", cg.Component, WsBridgeDevice)
_ENTITY_REF_DOMAINS = [
    (sensor.Sensor, ws_bridge_ns.class_("WsBridgeSensorRef", WsBridgeEntityRefBase)),
    (binary_sensor.BinarySensor, ws_bridge_ns.class_("WsBridgeBinarySensorRef", WsBridgeEntityRefBase)),
    (text_sensor.TextSensor, ws_bridge_ns.class_("WsBridgeTextSensorRef", WsBridgeEntityRefBase)),
    (switch.Switch, ws_bridge_ns.class_("WsBridgeSwitchRef", WsBridgeEntityRefBase)),
    (number.Number, ws_bridge_ns.class_("WsBridgeNumberRef", WsBridgeEntityRefBase)),
    (select.Select, ws_bridge_ns.class_("WsBridgeSelectRef", WsBridgeEntityRefBase)),
    (button.Button, ws_bridge_ns.class_("WsBridgeButtonRef", WsBridgeEntityRefBase)),
    (update.UpdateEntity, ws_bridge_ns.class_("WsBridgeUpdateRef", WsBridgeEntityRefBase)),
]

ENTITY_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WsBridgeEntityRefBase),
        # Loosely typed on purpose: the real domain (sensor/switch/...) isn't
        # known here, only that it must be *some* entity. ESPHome's own
        # IDPassValidationStep still rejects an id that isn't an entity at all
        # (e.g. pointing at the ws_bridge: hub itself) — see to_code_entity_ref().
        cv.Required(CONF_SOURCE_ID): cv.use_id(cg.EntityBase),
        # Both default to the source: unique_id to its YAML id (stable across
        # `name:` edits, unlike an object_id would be), name to its own name.
        cv.Optional(CONF_UNIQUE_ID): cv.string_strict,
        cv.Optional(CONF_NAME): cv.string_strict,
        cv.Optional(CONF_WS_DEVICE_ID): cv.string_strict,
        cv.Optional(CONF_WS_DEVICE_NAME): cv.string_strict,
    }
)


async def to_code_entity_ref(hub_var, conf):
    source_id = conf[CONF_SOURCE_ID]
    full_id, source_var = await cg.get_variable_with_full_id(source_id)

    ref_cls = None
    for domain_type, cls in _ENTITY_REF_DOMAINS:
        if isinstance(full_id.type, cg.MockObjClass) and full_id.type.inherits_from(domain_type):
            ref_cls = cls
            break
    if ref_cls is None:
        raise cv.Invalid(
            f"'{source_id.id}' (type {full_id.type}) is not an entity domain ws_bridge "
            "can expose via entities: — supported: sensor, binary_sensor, text_sensor, "
            "switch, number, select, button, update"
        )

    id_ = conf[CONF_ID]
    id_.type = ref_cls
    var = cg.new_Pvariable(id_)
    await cg.register_component(var, {})
    cg.add(var.set_ws_bridge_parent(hub_var))
    cg.add(var.set_unique_id(conf.get(CONF_UNIQUE_ID, source_id.id)))
    if CONF_WS_DEVICE_ID in conf:
        cg.add(var.set_device_id(conf[CONF_WS_DEVICE_ID]))
    if CONF_WS_DEVICE_NAME in conf:
        cg.add(var.set_device_name(conf[CONF_WS_DEVICE_NAME]))
    if CONF_NAME in conf:
        cg.add(var.set_name_override(conf[CONF_NAME]))
    cg.add(var.set_source(source_var))
    cg.add(hub_var.register_device(var))


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
            # After declaring everything on connect, send ws_bridge/sync with the
            # full set of unique_ids so HA drops entities this gateway no longer
            # provides. Entities still in the list keep their entity_id, history
            # and statistics — nothing is wiped and recreated.
            #
            # Off by default because it deletes HA-side entities. Safe to turn on
            # when every entity is declared by a ws_bridge platform, an
            # on_declare: lambda, or an on_connected: lambda — all three are
            # collected. Leave it off if anything declares itself later (from an
            # interval:, a button press, ...), since those would be absent from
            # the list and removed.
            cv.Optional(CONF_SYNC_ENTITIES, default=False): cv.boolean,
            # See ws_bridge.cpp's check_liveness_() for what these govern: an
            # app-level ping/pong that detects a peer that dropped without a
            # clean WS close, and a backstop that keeps retrying (2s doubling
            # to this cap, matching the companion hass-ble-android client's
            # HaWsClient) if we've simply been disconnected, for whenever
            # esp_websocket_client's own auto-reconnect doesn't recover on its
            # own (observed after e.g. HA itself restarting).
            cv.Optional(CONF_PING_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_PONG_TIMEOUT, default="15s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RECONNECT_TIMEOUT, default="30s"): cv.positive_time_period_milliseconds,
            # Periodically resends ws_bridge/connect + entity declarations even
            # while nominally connected. Needed because the transport (and
            # HA's generic websocket_api ping/pong) can stay alive while the
            # ws_bridge integration on the HA side loses track of this specific
            # gateway (e.g. its config entry reloaded independently of the raw
            # connection) — that's invisible to ping/pong since HA core answers
            # pings regardless of our integration's state, so state pushes
            # would otherwise be silently dropped forever with no disconnect
            # ever observed. 60s matches the companion hass-ble-android
            # client's HaWsClient.resubscribeJob, which hit and fixed the same
            # gap independently.
            cv.Optional(CONF_REANNOUNCE_INTERVAL, default="60s"): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_CONNECTED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ConnectedTrigger)}
            ),
            cv.Optional(CONF_ON_DISCONNECTED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DisconnectedTrigger)}
            ),
            # Fires alongside the registered platform entities' own
            # re-declaration — on connect and on every re-announce. This is
            # where hand-built send_entity_declare() calls belong (e.g. for
            # entity types ESPHome has no domain for, like device_tracker);
            # on_connected would skip the re-announce and leave them missing
            # after HA-side registration is healed.
            cv.Optional(CONF_ON_DECLARE): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DeclareTrigger)}
            ),
            cv.Optional(CONF_TRACKERS, default=[]): cv.ensure_list(TRACKER_SCHEMA),
            # Expose existing ESPHome entities without a parallel
            # `platform: ws_bridge` entity per value — see ENTITY_SCHEMA above
            # and the README's "Exposing existing entities" section.
            cv.Optional(CONF_ENTITIES, default=[]): cv.ensure_list(ENTITY_SCHEMA),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_esp_idf,
)

# Shared schema every ws_bridge platform (sensor/binary_sensor/switch/number/
# select/button/update) must extend, mirroring uartex's UARTEX_DEVICE_SCHEMA pattern.
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
    cg.add(var.set_sync_entities(config[CONF_SYNC_ENTITIES]))
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
    for conf in config.get(CONF_ON_DECLARE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_TRACKERS, []):
        tracker = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(tracker, conf)
        cg.add(tracker.set_ws_bridge_parent(var))
        cg.add(tracker.set_unique_id(conf[CONF_UNIQUE_ID]))
        if CONF_WS_DEVICE_ID in conf:
            cg.add(tracker.set_device_id(conf[CONF_WS_DEVICE_ID]))
        if CONF_WS_DEVICE_NAME in conf:
            cg.add(tracker.set_device_name(conf[CONF_WS_DEVICE_NAME]))
        cg.add(var.register_device(tracker))
        cg.add(tracker.set_name(conf[CONF_NAME]))
        if CONF_ICON in conf:
            cg.add(tracker.set_icon(conf[CONF_ICON]))
        lat_template = await cg.templatable(conf[CONF_LATITUDE], [], cg.float_)
        cg.add(tracker.set_latitude(lat_template))
        lon_template = await cg.templatable(conf[CONF_LONGITUDE], [], cg.float_)
        cg.add(tracker.set_longitude(lon_template))
        if CONF_GPS_ACCURACY in conf:
            acc_template = await cg.templatable(conf[CONF_GPS_ACCURACY], [], cg.float_)
            cg.add(tracker.set_gps_accuracy(acc_template))

    for conf in config.get(CONF_ENTITIES, []):
        await to_code_entity_ref(var, conf)
