from email.policy import default
import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, sensor
from esphome.components.text_sensor import register_text_sensor
from esphome.components.i2s_audio import microphone
from esphome import automation, pins, core
from esphome.const import CONF_ID, CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, CONF_UNIT_OF_MEASUREMENT, ICON_NEW_BOX, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP 
from esphome.core import coroutine
from esphome.util import SimpleRegistry
from .const import CONF_125, CONF_250, CONF_500, CONF_1K, CONF_2K, CONF_4K, CONF_8K, CONF_16K

_LOGGER = logging.getLogger(__name__)
AUTO_LOAD = ["text_sensor", "sensor"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["i2s_audio"]
microphone_fft_ns = cg.esphome_ns.namespace('microphone_fft')
MicrophoneFFT = microphone_fft_ns.class_('MicrophoneFFT', cg.Component, microphone.I2SAudioMicrophone)

MULTI_CONF = False

# microphone_fft Schema
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(MicrophoneFFT),
    cv.Optional(CONF_VERSION, default={CONF_NAME: "Version"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
    cv.Optional(CONF_125, default={CONF_NAME: CONF_125}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
        #cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
    cv.Optional(CONF_250, default={CONF_NAME: CONF_250}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
    }),
    cv.Optional(CONF_500, default={CONF_NAME: CONF_500}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
    }),
    cv.Optional(CONF_1K, default={CONF_NAME: CONF_1K}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
    }),
    cv.Optional(CONF_2K, default={CONF_NAME: CONF_2K}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
    }),
    cv.Optional(CONF_4K, default={CONF_NAME: CONF_4K}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
    }),
    cv.Optional(CONF_8K, default={CONF_NAME: CONF_8K}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
    }),
    cv.Optional(CONF_16K, default={CONF_NAME: CONF_16K}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
    }),
}).extend(cv.COMPONENT_SCHEMA).extend(microphone.MICROPHONE_SCHEMA)
)

async def to_code(config):
    cg.add_global(microphone_fft_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await microphone.register_microphone(var, config)
    if CONF_VERSION in config:
        sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
        await register_text_sensor(sens, config[CONF_VERSION])
        cg.add(var.set_version(sens))
    if CONF_125 in config:
        sens = cg.new_Pvariable(config[CONF_125][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_125])
        cg.add(var.set_125(sens))
    if CONF_250 in config:
        sens = cg.new_Pvariable(config[CONF_250][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_250])
        cg.add(var.set_250(sens))
    if CONF_500 in config:
        sens = cg.new_Pvariable(config[CONF_500][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_500])
        cg.add(var.set_500(sens))
    if CONF_1K in config:
        sens = cg.new_Pvariable(config[CONF_1K][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_1K])
        cg.add(var.set_1k(sens))
    if CONF_2K in config:
        sens = cg.new_Pvariable(config[CONF_2K][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_2K])
        cg.add(var.set_2k(sens))
    if CONF_4K in config:
        sens = cg.new_Pvariable(config[CONF_4K][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_4K])
        cg.add(var.set_4k(sens))
    if CONF_8K in config:
        sens = cg.new_Pvariable(config[CONF_8K][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_8K])
        cg.add(var.set_8k(sens))
    if CONF_16K in config:
        sens = cg.new_Pvariable(config[CONF_16K][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_16K])
        cg.add(var.set_16k(sens))
    cg.add_library("arduinoFFT", None)
#HEX_SCHEMA_REGISTRY = SimpleRegistry()
