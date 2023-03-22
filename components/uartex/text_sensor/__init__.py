import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import text_sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import uartex_ns, UARTExComponent, uint8_ptr_const, uint16_const, \
    state_schema, command_hex_schema
from ..const import CONF_UARTEX_ID, CONF_SUB_FILTER, CONF_FILTER, CONF_COMMAND_UPDATE

DEPENDENCIES = ['uartex']
UARTExTextSensor = uartex_ns.class_(
    'UARTExTextSensor', text_sensor.TextSensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExTextSensor),
    cv.GenerateID(CONF_UARTEX_ID): cv.declare_id(UARTExComponent),
    cv.Required(CONF_FILTER): state_schema,
    cv.Optional(CONF_SUB_FILTER): state_schema,
    cv.Optional(CONF_COMMAND_UPDATE): command_hex_schema,
    cv.Required(CONF_LAMBDA): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s')))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uartex.register_uartex_device(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [(uint8_ptr_const, 'data'),
                                                                  (uint16_const, 'len')],
                                            return_type=cg.optional.template(cg.const_char_ptr))
        cg.add(var.set_template(template_))

    yield text_sensor.register_text_sensor(var, config)
