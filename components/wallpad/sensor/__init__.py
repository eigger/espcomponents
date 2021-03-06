import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import sensor, wallpad
from esphome.const import CONF_ID, CONF_DEVICE, CONF_LAMBDA, CONF_DATA, CONF_UPDATE_INTERVAL, \
    UNIT_EMPTY, ICON_EMPTY, CONF_OFFSET, CONF_ACCURACY_DECIMALS
from .. import wallpad_ns, WallPadComponent, uint8_ptr_const, num_t_const, \
    state_hex_schema, command_hex_schema, STATE_NUM_SCHEMA
from ..const import CONF_WALLPAD_ID, CONF_SUB_DEVICE, CONF_COMMAND_STATE, CONF_LENGTH, CONF_PRECISION

DEPENDENCIES = ['wallpad']
WallPadSensor = wallpad_ns.class_(
    'WallPadSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(WallPadSensor),
    cv.GenerateID(CONF_WALLPAD_ID): cv.use_id(WallPadComponent),
    cv.Required(CONF_DEVICE): state_hex_schema,
    cv.Optional(CONF_SUB_DEVICE): state_hex_schema,
    cv.Optional(CONF_COMMAND_STATE): command_hex_schema,
    cv.Optional(CONF_LAMBDA): cv.returning_lambda,
    cv.Optional(CONF_DATA): STATE_NUM_SCHEMA
}).extend(cv.polling_component_schema('60s')), cv.has_exactly_one_key(CONF_LAMBDA, CONF_DATA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield wallpad.register_wallpad_device(var, config)

    if CONF_LAMBDA in config:
        template_ = yield cg.process_lambda(config[CONF_LAMBDA], [(uint8_ptr_const, 'data'),
                                                                  (num_t_const, 'len')],
                                            return_type=cg.optional.template(float))
        cg.add(var.set_template(template_))
    if CONF_DATA in config:
        data = config[CONF_DATA]
        data_ = yield data[CONF_OFFSET], data[CONF_LENGTH], data[CONF_PRECISION]
        cg.add(var.set_state_num(data_))
        config[CONF_ACCURACY_DECIMALS] = data[CONF_PRECISION]

    yield sensor.register_sensor(var, config)
