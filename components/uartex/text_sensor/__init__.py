import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import uartex_ns, UARTExDevice, \
    uartex_declare_type, \
    state_schema, state_string_expression, \
    command_hex_schema
from ..const import CONF_UARTEX_ID, CONF_STATE, CONF_COMMAND_UPDATE

DEPENDENCIES = ['uartex']
UARTExTextSensor = uartex_ns.class_('UARTExTextSensor', text_sensor.TextSensor, UARTExDevice)

CONFIG_SCHEMA = cv.All(text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExTextSensor),
    cv.GenerateID(CONF_UARTEX_ID): uartex_declare_type,
    cv.Optional(CONF_STATE): state_schema,
    cv.Optional(CONF_COMMAND_UPDATE): cv.templatable(command_hex_schema),
    cv.Required(CONF_LAMBDA): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s')))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_LAMBDA in config:
        state = await state_string_expression(config[CONF_LAMBDA])
        cg.add(var.set_state(CONF_LAMBDA, state))
    
