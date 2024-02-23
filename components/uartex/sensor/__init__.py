import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_UPDATE_INTERVAL, \
    UNIT_EMPTY, ICON_EMPTY, CONF_OFFSET, CONF_ACCURACY_DECIMALS
from .. import uartex_ns, uint8_ptr_const, uint16_const, \
    state_schema, command_hex_schema, _uartex_declare_type, STATE_NUM_SCHEMA
from ..const import CONF_UARTEX_ID, CONF_STATE, CONF_STATE_NUMBER, CONF_COMMAND_UPDATE, CONF_LENGTH, CONF_PRECISION

DEPENDENCIES = ['uartex']
UARTExSensor = uartex_ns.class_(
    'UARTExSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExSensor),
    cv.GenerateID(CONF_UARTEX_ID): _uartex_declare_type,
    cv.Required(CONF_STATE): state_schema,
    cv.Optional(CONF_COMMAND_UPDATE): command_hex_schema,
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
    cv.Optional(CONF_STATE_NUMBER): STATE_NUM_SCHEMA
}).extend(cv.polling_component_schema('60s')), cv.has_exactly_one_key(CONF_LAMBDA, CONF_STATE_NUMBER))


async def to_code(config):
    var = await cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_LAMBDA in config:
        template_ = await cg.process_lambda(config[CONF_LAMBDA], [(uint8_ptr_const, 'data'),
                                                                  (uint16_const, 'len')],
                                            return_type=cg.optional.template(float))
        cg.add(var.set_template(template_))
    if CONF_STATE_NUMBER in config:
        data = config[CONF_STATE_NUMBER]
        data_ = data[CONF_OFFSET], data[CONF_LENGTH], data[CONF_PRECISION]
        cg.add(var.set_state_num(data_))
        config[CONF_ACCURACY_DECIMALS] = data[CONF_PRECISION]

    await sensor.register_sensor(var, config)
