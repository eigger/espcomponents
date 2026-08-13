CONF_WS_BRIDGE_ID = "ws_bridge_id"

CONF_HOST = "host"
CONF_SSL = "ssl"
CONF_TOKEN = "token"
CONF_GATEWAY_ID = "gateway_id"
CONF_KEEP_LAST_STATE_ON_DISCONNECT = "keep_last_state_on_disconnect"
CONF_SYNC_ENTITIES = "sync_entities"

CONF_UNIQUE_ID = "unique_id"
# Prefixed with ws_ to avoid colliding with ESPHome's own reserved
# device_id/device (native multi-device grouping) entity schema keys.
CONF_WS_DEVICE_ID = "ws_device_id"
CONF_WS_DEVICE_NAME = "ws_device_name"

CONF_ON_CONNECTED = "on_connected"
CONF_ON_DISCONNECTED = "on_disconnected"
CONF_ON_DECLARE = "on_declare"

CONF_TRACKERS = "trackers"
CONF_GPS_ACCURACY = "gps_accuracy"
CONF_UPDATE_ID = "update_id"
CONF_BUTTON_ID = "button_id"
CONF_SENSOR_ID = "sensor_id"
CONF_BINARY_SENSOR_ID = "binary_sensor_id"
CONF_TEXT_SENSOR_ID = "text_sensor_id"
CONF_SWITCH_ID = "switch_id"
CONF_NUMBER_ID = "number_id"
CONF_SELECT_ID = "select_id"
CONF_LIGHT_ID = "light_id"
CONF_COVER_ID = "cover_id"
CONF_FAN_ID = "fan_id"

# Hub-side `entities:` list — exposes an already-existing ESPHome entity
# without a parallel `platform: ws_bridge` entity. See WsBridgeEntityRefBase.
CONF_ENTITIES = "entities"
CONF_SOURCE_ID = "source_id"

CONF_PING_INTERVAL = "ping_interval"
CONF_PONG_TIMEOUT = "pong_timeout"
CONF_RECONNECT_TIMEOUT = "reconnect_timeout"
CONF_REANNOUNCE_INTERVAL = "reannounce_interval"
