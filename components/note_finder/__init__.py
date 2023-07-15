from email.policy import default
import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor, number, sensor
from esphome.components.text_sensor import register_text_sensor
from esphome.components.i2s_audio import microphone
from esphome.components.microphone import register_microphone
from esphome import automation, pins, core
from esphome.const import CONF_ID, CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, CONF_UNIT_OF_MEASUREMENT, ICON_NEW_BOX, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP 
from esphome.core import coroutine
from esphome.util import SimpleRegistry
from .const import CONF_FREQUENCY, CONF_OCTAVE, CONF_NOTE

_LOGGER = logging.getLogger(__name__)
AUTO_LOAD = ["text_sensor", "number"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["microphone"]
note_finder_ns = cg.esphome_ns.namespace('note_finder')
NoteFinderComponent = note_finder_ns.class_('NoteFinderComponent', number.Number, cg.Component, microphone.I2SAudioMicrophone)

MULTI_CONF = False

# note_finder Schema

CONFIG_SCHEMA = cv.All(number.NUMBER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(NoteFinderComponent),
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
    cv.Optional(CONF_STEP, default=1): cv.float_,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=""): cv.string,
}).extend({
    cv.Optional(CONF_VERSION, default={CONF_NAME: "Version"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
    cv.Optional(CONF_NOTE, default={CONF_NAME: "Note"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default="mdi:alert-circle"): cv.icon,
    }),
    cv.Optional(CONF_FREQUENCY, default={CONF_NAME: "Frequency"}): sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(sensor.Sensor),
        cv.Optional(CONF_ICON, default="mdi:account"): cv.icon,
        cv.Optional(CONF_UNIT_OF_MEASUREMENT, default="Hz"): cv.string,
    }),
}).extend(cv.COMPONENT_SCHEMA).extend(microphone.BASE_SCHEMA))

async def to_code(config):
    cg.add_global(note_finder_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(            
        var,
        config,
        min_value = config[CONF_MIN_VALUE],
        max_value = config[CONF_MAX_VALUE],
        step = config[CONF_STEP],)
    await register_microphone(var, config)
    if CONF_VERSION in config:
        sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
        await register_text_sensor(sens, config[CONF_VERSION])
        cg.add(var.set_version(sens))
    if CONF_NOTE in config:
        sens = cg.new_Pvariable(config[CONF_NOTE][CONF_ID])
        await register_text_sensor(sens, config[CONF_NOTE])
        cg.add(var.set_note(sens))
    if CONF_FREQUENCY in config:
        sens = cg.new_Pvariable(config[CONF_FREQUENCY][CONF_ID])
        await sensor.register_sensor(sens, config[CONF_FREQUENCY])
        cg.add(var.set_frequency(sens))
    cg.add_library("arduinoFFT", None)
    
    
#HEX_SCHEMA_REGISTRY = SimpleRegistry()
