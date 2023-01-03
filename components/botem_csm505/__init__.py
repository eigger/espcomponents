from email.policy import default
import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor, number
from esphome.components.text_sensor import register_text_sensor
from esphome import automation, pins, core
from esphome.const import CONF_ID, CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, ICON_NEW_BOX, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP 
from esphome.core import coroutine
from esphome.util import SimpleRegistry
from .const import CONF_CS505_ID, CONF_ERROR

_LOGGER = logging.getLogger(__name__)
AUTO_LOAD = ["text_sensor", "number"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["uart"]
botem_csm505_ns = cg.esphome_ns.namespace('botem_csm505')
BotemCSM505Component = botem_csm505_ns.class_('BotemCSM505Component', number.Number, cg.Component, uart.UARTDevice)

MULTI_CONF = False

# botem_csm505 Schema

CONFIG_SCHEMA = cv.All(number.NUMBER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BotemCSM505Component),
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Optional(CONF_MIN_VALUE, default=0): cv.float_,
    cv.Optional(CONF_STEP, default=1): cv.float_,
}).extend({
    cv.Optional(CONF_VERSION, default={CONF_NAME: "Version"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
    cv.Optional(CONF_ERROR, default={CONF_NAME: "Error"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default="mdi:alert-circle"): cv.icon,
        #cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA))

async def to_code(config):
    cg.add_global(botem_csm505_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await number.register_number(            
        var,
        config,
        min_value = config[CONF_MIN_VALUE],
        max_value = config[CONF_MAX_VALUE],
        step = config[CONF_STEP],)
    if CONF_VERSION in config:
        sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
        await register_text_sensor(sens, config[CONF_VERSION])
        cg.add(var.set_version(sens))
    if CONF_ERROR in config:
        sens = cg.new_Pvariable(config[CONF_ERROR][CONF_ID])
        await register_text_sensor(sens, config[CONF_ERROR])
        cg.add(var.set_error(sens))
    
    
#HEX_SCHEMA_REGISTRY = SimpleRegistry()
