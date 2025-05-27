import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import uartex_ns, UARTExDevice, \
    state_num_schema, state_num_expression
from ..const import CONF_COMMAND_ON, CONF_COMMAND_OFF, CONF_STATE_ON, CONF_STATE_OFF, CONF_STATE_NUMBER

DEPENDENCIES = ['uartex']
UARTExSensor = uartex_ns.class_('UARTExSensor', sensor.Sensor, UARTExDevice)

CONFIG_SCHEMA = cv.All(sensor.sensor_schema(UARTExSensor).extend({
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE_NUMBER): cv.templatable(state_num_schema),
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Sensor do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Sensor do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Sensor do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Sensor do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA),  cv.has_exactly_one_key(CONF_LAMBDA, CONF_STATE_NUMBER))

async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_LAMBDA in config:
        state = await state_num_expression(config[CONF_LAMBDA])
        cg.add(var.set_state(CONF_LAMBDA, state))
        
    if CONF_STATE_NUMBER in config:
        state = await state_num_expression(config[CONF_STATE_NUMBER])
        cg.add(var.set_state(CONF_STATE_NUMBER, state))

    
