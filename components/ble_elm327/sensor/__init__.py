import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_MILLISECOND,
)
from .. import (
    ble_elm327_ns,
    BleElm327Component,
    BleElm327Device,
    BLE_ELM327_DEVICE_SCHEMA,
    register_ble_elm327_device,
    inject_preset,
)
from ..const import (
    CONF_BLE_ELM327_ID,
    CONF_TIMEOUT_COUNT,
    CONF_ERROR_COUNT,
    CONF_NO_DATA_COUNT,
    CONF_WRITE_FAILURE_COUNT,
    CONF_REINIT_COUNT,
    CONF_RECONNECT_COUNT,
    CONF_RESPONSE_TIME,
)

DEPENDENCIES = ["ble_elm327"]

BleElm327Sensor = ble_elm327_ns.class_("BleElm327Sensor", sensor.Sensor, BleElm327Device)

# inject_preset runs first: fills in pid/mode/formula/unit/etc. from OBD_PRESETS,
# then sensor_schema + BLE_ELM327_DEVICE_SCHEMA validate the completed config.
DEVICE_SCHEMA = cv.All(
    inject_preset,
    sensor.sensor_schema(BleElm327Sensor).extend(BLE_ELM327_DEVICE_SCHEMA),
)


# ── Diagnostic sensors (component health, not OBD data) ──────────────────────


def _counter_schema(icon):
    return sensor.sensor_schema(
        icon=icon,
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


def _response_time_schema(icon):
    return sensor.sensor_schema(
        icon=icon,
        unit_of_measurement=UNIT_MILLISECOND,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DURATION,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )


# key -> (setter on BleElm327Component, schema)
DIAGNOSTIC_SENSORS = {
    CONF_TIMEOUT_COUNT: ("set_timeout_count_sensor", _counter_schema("mdi:timer-alert-outline")),
    CONF_ERROR_COUNT: ("set_error_count_sensor", _counter_schema("mdi:alert-circle-outline")),
    CONF_NO_DATA_COUNT: ("set_no_data_count_sensor", _counter_schema("mdi:database-off-outline")),
    CONF_WRITE_FAILURE_COUNT: ("set_write_failure_count_sensor", _counter_schema("mdi:upload-off-outline")),
    CONF_REINIT_COUNT: ("set_reinit_count_sensor", _counter_schema("mdi:restart")),
    CONF_RECONNECT_COUNT: ("set_reconnect_count_sensor", _counter_schema("mdi:bluetooth-connect")),
    CONF_RESPONSE_TIME: ("set_response_time_sensor", _response_time_schema("mdi:timer-outline")),
}


DIAGNOSTIC_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLE_ELM327_ID): cv.use_id(BleElm327Component),
        **{
            cv.Optional(key): schema
            for key, (_, schema) in DIAGNOSTIC_SENSORS.items()
        },
    }
)


def _validate(config):
    """A platform entry is either a diagnostic block (any diagnostic key) or a PID sensor."""
    if isinstance(config, dict) and any(key in config for key in DIAGNOSTIC_SENSORS):
        return DIAGNOSTIC_SCHEMA(config)
    return DEVICE_SCHEMA(config)


CONFIG_SCHEMA = _validate


async def to_code(config):
    if any(key in config for key in DIAGNOSTIC_SENSORS):
        parent = await cg.get_variable(config[CONF_BLE_ELM327_ID])
        for key, (setter, _) in DIAGNOSTIC_SENSORS.items():
            if key in config:
                sens = await sensor.new_sensor(config[key])
                cg.add(getattr(parent, setter)(sens))
        return

    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)   # sets update_interval on the PollingComponent
    await register_ble_elm327_device(var, config)
