import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import STATE_CLASS_MEASUREMENT, DEVICE_CLASS_WEIGHT
from . import M5UnitScalesComponent, CONF_M5UNIT_SCALES_ID

DEPENDENCIES = ["m5unit_scales"]

CONF_WEIGHT = "weight"
CONF_RAW_ADC = "raw_adc"
CONF_ABSOLUTE_WEIGHT = "absolute_weight"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_M5UNIT_SCALES_ID): cv.use_id(M5UnitScalesComponent),
    cv.Optional(CONF_WEIGHT): sensor.sensor_schema(
        unit_of_measurement="g",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_WEIGHT,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:scale-balance",
    ),
    cv.Optional(CONF_RAW_ADC): sensor.sensor_schema(
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:sine-wave",
    ),
    cv.Optional(CONF_ABSOLUTE_WEIGHT): sensor.sensor_schema(
        unit_of_measurement="g",
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_WEIGHT,
        state_class=STATE_CLASS_MEASUREMENT,
        icon="mdi:scale-balance",
    ),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5UNIT_SCALES_ID])
    if CONF_WEIGHT in config:
        sens = await sensor.new_sensor(config[CONF_WEIGHT])
        cg.add(parent.set_weight_sensor(sens))
    if CONF_RAW_ADC in config:
        sens = await sensor.new_sensor(config[CONF_RAW_ADC])
        cg.add(parent.set_raw_adc_sensor(sens))
    if CONF_ABSOLUTE_WEIGHT in config:
        sens = await sensor.new_sensor(config[CONF_ABSOLUTE_WEIGHT])
        cg.add(parent.set_absolute_weight_sensor(sens))
