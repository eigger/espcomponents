import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import uartex_ns, UARTExDevice, \
    uartex_declare_type, \
    state_schema, state_num_schema, state_num_expression, \
    command_hex_schema
from ..const import CONF_UARTEX_ID, CONF_STATE, CONF_STATE_NUMBER, CONF_COMMAND_UPDATE

DEPENDENCIES = ['uartex']
UARTExSensor = uartex_ns.class_('UARTExSensor', sensor.Sensor, UARTExDevice)

CONFIG_SCHEMA = cv.All(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExSensor),
    cv.GenerateID(CONF_UARTEX_ID): uartex_declare_type,
    cv.Optional(CONF_STATE): state_schema,
    cv.Optional(CONF_COMMAND_UPDATE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
    cv.Optional(CONF_STATE_NUMBER): cv.templatable(state_num_schema),
}).extend(cv.polling_component_schema('60s')), cv.has_exactly_one_key(CONF_LAMBDA, CONF_STATE_NUMBER))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_LAMBDA in config:
        state = await state_num_expression(config[CONF_LAMBDA])
        cg.add(var.set_state(CONF_LAMBDA, state))
        
    if CONF_STATE_NUMBER in config:
        state = await state_num_expression(config[CONF_STATE_NUMBER])
        cg.add(var.set_state(CONF_STATE_NUMBER, state))

    
