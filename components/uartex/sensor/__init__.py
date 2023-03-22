import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_UPDATE_INTERVAL, \
    UNIT_EMPTY, ICON_EMPTY, CONF_OFFSET, CONF_ACCURACY_DECIMALS
from .. import uartex_ns, UARTExComponent, uint8_ptr_const, uint16_const, \
    state_schema, command_hex_schema, STATE_NUM_SCHEMA
from ..const import CONF_UARTEX_ID, CONF_SUB_FILTER, CONF_FILTER, CONF_STATE_NUMBER, CONF_COMMAND_UPDATE, CONF_LENGTH, CONF_PRECISION

DEPENDENCIES = ['uartex']
UARTExSensor = uartex_ns.class_(
    'UARTExSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExSensor),
    cv.GenerateID(CONF_UARTEX_ID): cv.declare_id(UARTExComponent),
    cv.Required(CONF_FILTER): state_schema,
    cv.Optional(CONF_SUB_FILTER): state_schema,
    cv.Optional(CONF_COMMAND_UPDATE): command_hex_schema,
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
    cv.Optional(CONF_STATE_NUMBER): STATE_NUM_SCHEMA
}).extend(cv.polling_component_schema('60s')), cv.has_exactly_one_key(CONF_LAMBDA, CONF_STATE_NUMBER))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uartex.register_uartex_device(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [(uint8_ptr_const, 'data'),
                                                                  (uint16_const, 'len')],
                                            return_type=cg.optional.template(float))
        cg.add(var.set_template(template_))
    if CONF_STATE_NUMBER in config:
        data = config[CONF_STATE_NUMBER]
        data_ = yield data[CONF_OFFSET], data[CONF_LENGTH], data[CONF_PRECISION]
        cg.add(var.set_state_num(data_))
        config[CONF_ACCURACY_DECIMALS] = data[CONF_PRECISION]

    yield sensor.register_sensor(var, config)
