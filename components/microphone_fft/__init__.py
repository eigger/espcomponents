from email.policy import default
import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, sensor, esp32
from esphome.components.text_sensor import register_text_sensor
from esphome.components.i2s_audio import microphone, CONF_I2S_AUDIO_ID, CONF_I2S_DIN_PIN
from esphome.components.i2s_audio.microphone import CONF_ADC_TYPE, CONF_ADC_PIN, CONF_PDM, CONF_BITS_PER_SAMPLE
from esphome.components.microphone import register_microphone
from esphome.components.adc import ESP32_VARIANT_ADC1_PIN_TO_CHANNEL, validate_adc_pin
from esphome import automation, pins, core
from esphome.const import CONF_ID, CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, ICON_NEW_BOX, CONF_NUMBER, CONF_CHANNEL 
from esphome.core import coroutine
from esphome.util import SimpleRegistry
from .const import CONF_MAX_FREQUENCY

_LOGGER = logging.getLogger(__name__)
AUTO_LOAD = ["text_sensor", "sensor"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["i2s_audio"]
microphone_fft_ns = cg.esphome_ns.namespace('microphone_fft')
MicrophoneFFT = microphone_fft_ns.class_('MicrophoneFFT', microphone.I2SAudioMicrophone)

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
    cv.Optional(CONF_MAX_FREQUENCY, default={CONF_NAME: CONF_MAX_FREQUENCY}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:sine-wave"): cv.icon,
        #cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
}).extend(microphone.I2SAudioMicrophone.CONFIG_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await cg.register_parented(var, config[CONF_I2S_AUDIO_ID])

    if config[CONF_ADC_TYPE] == "internal":
        variant = esp32.get_esp32_variant()
        pin_num = config[CONF_ADC_PIN][CONF_NUMBER]
        channel = ESP32_VARIANT_ADC1_PIN_TO_CHANNEL[variant][pin_num]
        cg.add(var.set_adc_channel(channel))
    else:
        cg.add(var.set_din_pin(config[CONF_I2S_DIN_PIN]))
        cg.add(var.set_pdm(config[CONF_PDM]))

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_bits_per_sample(config[CONF_BITS_PER_SAMPLE]))

    await microphone.register_microphone(var, config)
    if CONF_VERSION in config:
        sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
        await register_text_sensor(sens, config[CONF_VERSION])
        cg.add(var.set_version(sens))
    if CONF_MAX_FREQUENCY in config:
        sens = cg.new_Pvariable(config[CONF_MAX_FREQUENCY][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_MAX_FREQUENCY])
        cg.add(var.set_max_frequency(sens))
    cg.add_library("arduinoFFT", None)
#HEX_SCHEMA_REGISTRY = SimpleRegistry()
