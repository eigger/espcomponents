"""gps_compass: a true-north heading from a magnetometer + accelerometer + GPS, calibrated by driving.  See README.md.

The component learns the magnetometer calibration (hard iron, field projection, pitch/roll coupling) from the GPS
course while the vehicle moves, so no figure-8 calibration is needed.  The math is header-only (compass_math.h) and
unit-tested on the host; this file is the schema and the codegen.
"""

import esphome.codegen as cg
from esphome.components import binary_sensor, gps, sensor, text_sensor
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_MEASUREMENT_ANGLE,
    UNIT_DEGREES,
)

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["gps"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor"]

gps_compass_ns = cg.esphome_ns.namespace("gps_compass")
GpsCompass = gps_compass_ns.class_("GpsCompass", cg.Component, gps.GPSListener)
CalibratedTrigger = gps_compass_ns.class_("CalibratedTrigger", automation.Trigger.template(cg.bool_, cg.uint32))
SetLevelAction = gps_compass_ns.class_("SetLevelAction", automation.Action)
ClearLevelAction = gps_compass_ns.class_("ClearLevelAction", automation.Action)
ResetCalibrationAction = gps_compass_ns.class_("ResetCalibrationAction", automation.Action)

CONF_GPS_ID = "gps_id"
CONF_MAGNETOMETER = "magnetometer"
CONF_ACCELEROMETER = "accelerometer"
CONF_X = "x"
CONF_Y = "y"
CONF_Z = "z"
CONF_FORWARD_AXIS = "forward_axis"
CONF_RIGHT_AXIS = "right_axis"
CONF_PITCH_OFFSET = "pitch_offset"
CONF_ROLL_OFFSET = "roll_offset"
CONF_REVERSE = "reverse"
CONF_MIN_LEARNING_SPEED = "min_learning_speed"
CONF_MOVING_SPEED = "moving_speed"
CONF_STOPPED_SPEED = "stopped_speed"
CONF_MAX_HDOP = "max_hdop"
CONF_MIN_SATELLITES = "min_satellites"
CONF_FIELD_FILTER = "field_filter"
CONF_MIN_FIELD_STRENGTH = "min_field_strength"
CONF_SENSOR_TIMEOUT = "sensor_timeout"
CONF_GPS_TIMEOUT = "gps_timeout"
CONF_OBSERVATION_INTERVAL = "observation_interval"
CONF_FIT_INTERVAL = "fit_interval"
CONF_MIN_SECTORS = "min_sectors"
CONF_MAX_SECTOR_GAP = "max_sector_gap"
CONF_MIN_NEW_SAMPLES = "min_new_samples"
CONF_HEADING = "heading"
CONF_MAGNETIC_HEADING = "magnetic_heading"
CONF_GPS_HEADING = "gps_heading"
CONF_PITCH = "pitch"
CONF_ROLL = "roll"
CONF_SAMPLES = "samples"
CONF_SOURCE = "source"
CONF_STATUS = "status"
CONF_CALIBRATED = "calibrated"
CONF_ON_CALIBRATED = "on_calibrated"

_speed = cv.All(cv.float_with_unit("speed", "(km/h|kph)", optional_unit=True), cv.float_range(min=0.0, max=400.0))
_level_angle = cv.All(cv.angle, cv.float_range(min=-90.0, max=90.0))
_time = cv.All(cv.positive_time_period_milliseconds, cv.Range(max=cv.TimePeriod(hours=1)))

_AXIS = cv.one_of("X", "+X", "-X", "Y", "+Y", "-Y", "Z", "+Z", "-Z", upper=True)

_XYZ_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_X): cv.use_id(sensor.Sensor),
        cv.Required(CONF_Y): cv.use_id(sensor.Sensor),
        cv.Required(CONF_Z): cv.use_id(sensor.Sensor),
    }
)


def _split_axis(text):
    """'-Z' -> (2, -1.0); 'x' -> (0, 1.0)."""
    sign = -1.0 if text.startswith("-") else 1.0
    return "XYZ".index(text[-1]), sign


def _validate(config):
    if _split_axis(config[CONF_FORWARD_AXIS])[0] == _split_axis(config[CONF_RIGHT_AXIS])[0]:
        raise cv.Invalid(f"'{CONF_FORWARD_AXIS}' and '{CONF_RIGHT_AXIS}' must be different axes")
    if config[CONF_MOVING_SPEED] < config[CONF_STOPPED_SPEED]:
        raise cv.Invalid(f"'{CONF_MOVING_SPEED}' must not be below '{CONF_STOPPED_SPEED}'")
    if config[CONF_MIN_LEARNING_SPEED] < config[CONF_MOVING_SPEED]:
        # Observations need the vehicle to count as moving, so a lower value could never apply.
        raise cv.Invalid(f"'{CONF_MIN_LEARNING_SPEED}' must not be below '{CONF_MOVING_SPEED}'")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(GpsCompass),
            cv.Required(CONF_GPS_ID): cv.use_id(gps.GPS),
            cv.Optional(CONF_MAGNETOMETER): _XYZ_SCHEMA,
            cv.Optional(CONF_ACCELEROMETER): _XYZ_SCHEMA,
            cv.Optional(CONF_FORWARD_AXIS, default="+Y"): _AXIS,
            cv.Optional(CONF_RIGHT_AXIS, default="+X"): _AXIS,
            cv.Optional(CONF_PITCH_OFFSET, default=0.0): _level_angle,
            cv.Optional(CONF_ROLL_OFFSET, default=0.0): _level_angle,
            cv.Optional(CONF_REVERSE, default=False): cv.templatable(cv.boolean),
            cv.Optional(CONF_MIN_LEARNING_SPEED, default=18.0): _speed,
            cv.Optional(CONF_MOVING_SPEED, default=8.0): _speed,
            cv.Optional(CONF_STOPPED_SPEED, default=3.0): _speed,
            cv.Optional(CONF_MAX_HDOP, default=3.0): cv.float_range(min=0.1, max=50.0),
            cv.Optional(CONF_MIN_SATELLITES, default=4): cv.int_range(min=1, max=32),
            cv.Optional(CONF_FIELD_FILTER, default=0.35): cv.float_range(min=0.01, max=1.0),
            cv.Optional(CONF_MIN_FIELD_STRENGTH, default=2.0): cv.float_range(min=0.0, max=1000.0),
            cv.Optional(CONF_SENSOR_TIMEOUT, default="1500ms"): _time,
            cv.Optional(CONF_GPS_TIMEOUT, default="3s"): _time,
            cv.Optional(CONF_OBSERVATION_INTERVAL, default="900ms"): _time,
            cv.Optional(CONF_FIT_INTERVAL, default="3s"): _time,
            cv.Optional(CONF_MIN_SECTORS, default=5): cv.int_range(min=1, max=8),
            cv.Optional(CONF_MAX_SECTOR_GAP, default=3): cv.int_range(min=1, max=8),
            cv.Optional(CONF_MIN_NEW_SAMPLES, default=4): cv.int_range(min=0, max=40),
            cv.Optional(CONF_HEADING): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon="mdi:compass",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT_ANGLE,
            ),
            cv.Optional(CONF_MAGNETIC_HEADING): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon="mdi:compass-outline",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT_ANGLE,
            ),
            cv.Optional(CONF_GPS_HEADING): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon="mdi:crosshairs-gps",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT_ANGLE,
            ),
            cv.Optional(CONF_PITCH): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon="mdi:rotate-right",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ROLL): sensor.sensor_schema(
                unit_of_measurement=UNIT_DEGREES,
                icon="mdi:rotate-left",
                accuracy_decimals=1,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SAMPLES): sensor.sensor_schema(
                icon="mdi:counter",
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_SOURCE): text_sensor.text_sensor_schema(icon="mdi:compass-rose"),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(icon="mdi:progress-wrench"),
            cv.Optional(CONF_CALIBRATED): binary_sensor.binary_sensor_schema(icon="mdi:check-decagram"),
            cv.Optional(CONF_ON_CALIBRATED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CalibratedTrigger)}
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await gps.register_listener(await cg.get_variable(config[CONF_GPS_ID]), var)

    cg.add(var.set_preference_key(config[CONF_ID].id))
    for key, setter in ((CONF_MAGNETOMETER, "set_magnetometer"), (CONF_ACCELEROMETER, "set_accelerometer")):
        if conf := config.get(key):
            cg.add(
                getattr(var, setter)(
                    await cg.get_variable(conf[CONF_X]),
                    await cg.get_variable(conf[CONF_Y]),
                    await cg.get_variable(conf[CONF_Z]),
                )
            )
    cg.add(var.set_forward_axis(*_split_axis(config[CONF_FORWARD_AXIS])))
    cg.add(var.set_right_axis(*_split_axis(config[CONF_RIGHT_AXIS])))
    cg.add(var.set_initial_level(config[CONF_PITCH_OFFSET], config[CONF_ROLL_OFFSET]))
    if (reverse := config[CONF_REVERSE]) is not False:
        cg.add(var.set_reverse(await cg.templatable(reverse, [], bool)))
    cg.add(var.set_min_learning_speed(config[CONF_MIN_LEARNING_SPEED]))
    cg.add(var.set_moving_speed(config[CONF_MOVING_SPEED]))
    cg.add(var.set_stopped_speed(config[CONF_STOPPED_SPEED]))
    cg.add(var.set_max_hdop(config[CONF_MAX_HDOP]))
    cg.add(var.set_min_satellites(config[CONF_MIN_SATELLITES]))
    cg.add(var.set_field_filter(config[CONF_FIELD_FILTER]))
    cg.add(var.set_min_field_strength(config[CONF_MIN_FIELD_STRENGTH]))
    cg.add(var.set_sensor_timeout(config[CONF_SENSOR_TIMEOUT].total_milliseconds))
    cg.add(var.set_gps_timeout(config[CONF_GPS_TIMEOUT].total_milliseconds))
    cg.add(var.set_observation_interval(config[CONF_OBSERVATION_INTERVAL].total_milliseconds))
    cg.add(var.set_fit_interval(config[CONF_FIT_INTERVAL].total_milliseconds))
    cg.add(var.set_min_sectors(config[CONF_MIN_SECTORS]))
    cg.add(var.set_max_sector_gap(config[CONF_MAX_SECTOR_GAP]))
    cg.add(var.set_min_new_samples(config[CONF_MIN_NEW_SAMPLES]))

    for key, setter in (
        (CONF_HEADING, "set_heading_sensor"),
        (CONF_MAGNETIC_HEADING, "set_magnetic_heading_sensor"),
        (CONF_GPS_HEADING, "set_gps_heading_sensor"),
        (CONF_PITCH, "set_pitch_sensor"),
        (CONF_ROLL, "set_roll_sensor"),
        (CONF_SAMPLES, "set_samples_sensor"),
    ):
        if conf := config.get(key):
            cg.add(getattr(var, setter)(await sensor.new_sensor(conf)))
    for key, setter in ((CONF_SOURCE, "set_source_text_sensor"), (CONF_STATUS, "set_status_text_sensor")):
        if conf := config.get(key):
            cg.add(getattr(var, setter)(await text_sensor.new_text_sensor(conf)))
    if conf := config.get(CONF_CALIBRATED):
        cg.add(var.set_calibrated_binary_sensor(await binary_sensor.new_binary_sensor(conf)))

    for conf in config.get(CONF_ON_CALIBRATED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.bool_, "first"), (cg.uint32, "samples")], conf)


_PARENT = {cv.GenerateID(): cv.use_id(GpsCompass)}


async def _action(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parent(paren))
    return var


automation.register_action(
    "gps_compass.clear_level", ClearLevelAction, automation.maybe_simple_id(_PARENT), synchronous=True
)(_action)
automation.register_action(
    "gps_compass.reset_calibration", ResetCalibrationAction, automation.maybe_simple_id(_PARENT), synchronous=True
)(_action)


@automation.register_action(
    "gps_compass.set_level",
    SetLevelAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(GpsCompass),
            cv.Optional(CONF_PITCH): cv.templatable(_level_angle),
            cv.Optional(CONF_ROLL): cv.templatable(_level_angle),
        }
    ),
    synchronous=True,
)
async def set_level_to_code(config, action_id, template_arg, args):
    var = await _action(config, action_id, template_arg, args)
    for key in (CONF_PITCH, CONF_ROLL):
        if key in config:
            cg.add(getattr(var, f"set_{key}")(await cg.templatable(config[key], args, float)))
    return var
