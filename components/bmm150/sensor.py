from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DURATION,
    CONF_HEADING,
    CONF_ID,
    ICON_MAGNET,
    ICON_SCREEN_ROTATION,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREES,
    UNIT_MICROTESLA,
)

CODEOWNERS = ["@eigger"]

DEPENDENCIES = ["i2c"]

CONF_MAG_X = "magnetic_field_x"
CONF_MAG_Y = "magnetic_field_y"
CONF_MAG_Z = "magnetic_field_z"
CONF_ACCEL_X_ID = "accel_x_id"
CONF_ACCEL_Y_ID = "accel_y_id"
CONF_ACCEL_Z_ID = "accel_z_id"
CONF_DECLINATION = "declination"
CONF_HEADING_OFFSET = "heading_offset"
CONF_SOFT_IRON = "soft_iron"
CONF_CALIBRATION_MODE = "calibration_mode"
CONF_MAG_AXES = "mag_axes"
CONF_ACCEL_AXES = "accel_axes"
CONF_ON_CALIBRATION_FINISHED = "on_calibration_finished"

AXIS_MAP = {
    "x": (0, 1),
    "y": (1, 1),
    "z": (2, 1),
    "-x": (0, -1),
    "-y": (1, -1),
    "-z": (2, -1),
}

bmm150_ns = cg.esphome_ns.namespace("bmm150")
BMM150Component = bmm150_ns.class_("BMM150Component", cg.PollingComponent, i2c.I2CDevice)
CalibrateAction = bmm150_ns.class_("CalibrateAction", automation.Action)
CalibrationMode = bmm150_ns.enum("CalibrationMode")
CALIBRATION_MODES = {
    "yaw": CalibrationMode.CALIBRATION_MODE_YAW,
    "full": CalibrationMode.CALIBRATION_MODE_FULL,
}


def validate_axes(value):
    value = cv.ensure_list(cv.one_of(*AXIS_MAP, lower=True))(value)
    if len(value) != 3:
        raise cv.Invalid("Must specify exactly 3 axes")
    indices = sorted(AXIS_MAP[v][0] for v in value)
    if indices != [0, 1, 2]:
        raise cv.Invalid("Axes must be a permutation of x, y, z (optional leading '-')")
    return value


def validate_accel(config):
    keys = (CONF_ACCEL_X_ID, CONF_ACCEL_Y_ID, CONF_ACCEL_Z_ID)
    present = [k in config for k in keys]
    if any(present) and not all(present):
        raise cv.Invalid("accel_x_id, accel_y_id and accel_z_id must all be set together")
    return config


def axes_to_args(axes):
    args = []
    for name in axes:
        src, sign = AXIS_MAP[name]
        args.extend((src, sign))
    return args


mag_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_MICROTESLA,
    icon=ICON_MAGNET,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)
heading_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    icon=ICON_SCREEN_ROTATION,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BMM150Component),
            cv.Optional(CONF_MAG_X): mag_schema,
            cv.Optional(CONF_MAG_Y): mag_schema,
            cv.Optional(CONF_MAG_Z): mag_schema,
            cv.Optional(CONF_HEADING): heading_schema,
            cv.Optional(CONF_ACCEL_X_ID): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_ACCEL_Y_ID): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_ACCEL_Z_ID): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_DECLINATION, default=0.0): cv.float_,
            cv.Optional(CONF_HEADING_OFFSET, default=0.0): cv.float_,
            cv.Optional(CONF_SOFT_IRON, default=False): cv.boolean,
            cv.Optional(CONF_CALIBRATION_MODE, default="yaw"): cv.enum(CALIBRATION_MODES, lower=True),
            cv.Optional(CONF_MAG_AXES, default=["x", "y", "z"]): validate_axes,
            cv.Optional(CONF_ACCEL_AXES, default=["x", "y", "z"]): validate_axes,
            cv.Optional(CONF_ON_CALIBRATION_FINISHED): automation.validate_automation(single=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x10)),
    validate_accel,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_MAG_X in config:
        sens = await sensor.new_sensor(config[CONF_MAG_X])
        cg.add(var.set_mag_x(sens))
    if CONF_MAG_Y in config:
        sens = await sensor.new_sensor(config[CONF_MAG_Y])
        cg.add(var.set_mag_y(sens))
    if CONF_MAG_Z in config:
        sens = await sensor.new_sensor(config[CONF_MAG_Z])
        cg.add(var.set_mag_z(sens))
    if CONF_HEADING in config:
        sens = await sensor.new_sensor(config[CONF_HEADING])
        cg.add(var.set_heading(sens))

    if CONF_ACCEL_X_ID in config:
        cg.add(var.set_accel_x(await cg.get_variable(config[CONF_ACCEL_X_ID])))
        cg.add(var.set_accel_y(await cg.get_variable(config[CONF_ACCEL_Y_ID])))
        cg.add(var.set_accel_z(await cg.get_variable(config[CONF_ACCEL_Z_ID])))

    cg.add(var.set_declination(config[CONF_DECLINATION]))
    cg.add(var.set_heading_offset(config[CONF_HEADING_OFFSET]))
    cg.add(var.set_soft_iron(config[CONF_SOFT_IRON]))
    cg.add(var.set_calibration_mode(config[CONF_CALIBRATION_MODE]))
    cg.add(var.set_mag_axes(*axes_to_args(config[CONF_MAG_AXES])))
    cg.add(var.set_accel_axes(*axes_to_args(config[CONF_ACCEL_AXES])))

    if CONF_ON_CALIBRATION_FINISHED in config:
        await automation.build_automation(
            var.get_calibration_finished_trigger(),
            [(cg.bool_, "success")],
            config[CONF_ON_CALIBRATION_FINISHED],
        )


@automation.register_action(
    "bmm150.calibrate",
    CalibrateAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(BMM150Component),
            cv.Optional(CONF_DURATION, default="30s"): cv.positive_time_period_milliseconds,
        }
    ),
    synchronous=True,
)
async def bmm150_calibrate_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_duration(config[CONF_DURATION]))
    return var
