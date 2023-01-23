import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import text_sensor, bluetoothex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import bluetoothex_ns, BluetoothExComponent, uint8_ptr_const, uint16_const, \
    state_schema, command_hex_schema
from ..const import CONF_BLUETOOTHEX_ID, CONF_SUB_FILTER, CONF_FILTER, CONF_COMMAND_UPDATE

DEPENDENCIES = ['bluetoothex']
BluetoothExTextSensor = bluetoothex_ns.class_(
    'BluetoothExTextSensor', text_sensor.TextSensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BluetoothExTextSensor),
    cv.GenerateID(CONF_BLUETOOTHEX_ID): cv.use_id(BluetoothExComponent),
    cv.Required(CONF_FILTER): state_schema,
    cv.Optional(CONF_SUB_FILTER): state_schema,
    cv.Optional(CONF_COMMAND_UPDATE): command_hex_schema,
    cv.Required(CONF_LAMBDA): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s')))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield bluetoothex.register_bluetoothex_device(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [(uint8_ptr_const, 'data'),
                                                                  (uint16_const, 'len')],
                                            return_type=cg.optional.template(cg.const_char_ptr))
        cg.add(var.set_template(template_))

    yield text_sensor.register_text_sensor(var, config)
