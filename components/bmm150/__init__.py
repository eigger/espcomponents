import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID,\
    STATE_CLASS_MEASUREMENT

DEPENDENCIES = ['i2c']
CONF_MAG_X = 'magnetic_field_x'
CONF_MAG_Y = 'magnetic_field_y'
CONF_MAG_Z = 'magnetic_field_z'

bmm150_ns = cg.esphome_ns.namespace('bmm150')
BMM150Component = bmm150_ns.class_('BMM150Component', cg.PollingComponent, i2c.I2CDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BMM150Component),
    cv.Optional(CONF_MAG_X): sensor.sensor_schema(
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MAG_Y): sensor.sensor_schema(
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_MAG_Z): sensor.sensor_schema(
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}).extend(cv.polling_component_schema("60s")).extend(i2c.i2c_device_schema(0x10))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_MAG_X in config:
        conf = config[CONF_MAG_X]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_mag_x(sens))
    if CONF_MAG_Y in config:
        conf = config[CONF_MAG_Y]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_mag_y(sens))
    if CONF_MAG_Z in config:
        conf = config[CONF_MAG_Z]
        sens = await sensor.new_sensor(conf)
        cg.add(var.set_mag_z(sens))

