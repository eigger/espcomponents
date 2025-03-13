import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_OFFSET, CONF_ACCURACY_DECIMALS
from .. import uartex_ns, uint8_ptr_const, uint16_const, \
    state_schema, command_hex_schema, state_num_schema, _uartex_declare_type
from ..const import CONF_STATE, CONF_STATE_NUMBER, CONF_COMMAND_UPDATE, CONF_LENGTH, CONF_PRECISION, CONF_UARTEX_ID, CONF_SIGNED, CONF_ENDIAN

DEPENDENCIES = ['uartex']
UARTExSensor = uartex_ns.class_('UARTExSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExSensor),
    cv.GenerateID(CONF_UARTEX_ID): _uartex_declare_type,
    cv.Required(CONF_STATE): state_schema,
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
        template_ = await cg.templatable(config[CONF_LAMBDA], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
        cg.add(var.set_state(CONF_LAMBDA, template_))
        
    if CONF_STATE_NUMBER in config:
        state = config[CONF_STATE_NUMBER]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state(CONF_STATE_NUMBER, templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION], state[CONF_SIGNED], state[CONF_ENDIAN]
            cg.add(var.set_state(CONF_STATE_NUMBER, args))
            config[CONF_ACCURACY_DECIMALS] = state[CONF_PRECISION]

    
