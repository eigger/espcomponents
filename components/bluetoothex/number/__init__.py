import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, bluetoothex
from esphome.const import CONF_ID, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP, CONF_OFFSET
from .. import bluetoothex_ns, cmd_t, uint8_ptr_const, uint16_const, STATE_NUM_SCHEMA
from ..const import CONF_COMMAND_NUMBER, CONF_COMMAND_OFF, CONF_STATE_NUMBER, CONF_STATE_OFF, \
    CONF_COMMAND_ON, CONF_STATE_ON, CONF_LENGTH, CONF_PRECISION

DEPENDENCIES = ['bluetoothex']
BluetoothExNumber = bluetoothex_ns.class_('BluetoothExNumber', number.Number, cg.Component)

CONFIG_SCHEMA = cv.All(number.NUMBER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(BluetoothExNumber),
    cv.Required(CONF_MIN_VALUE): cv.float_,
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Required(CONF_STEP): cv.float_,
    cv.Required(CONF_STATE_NUMBER): cv.templatable(STATE_NUM_SCHEMA),
    cv.Required(CONF_COMMAND_NUMBER): cv.returning_lambda,
}).extend(bluetoothex.BLUETOOTHEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("BluetoothEx Number do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("BluetoothEx Number do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("BluetoothEx Number do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("BluetoothEx Number do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield number.register_number(            
        var,
        config,
        min_value = config[CONF_MIN_VALUE],
        max_value = config[CONF_MAX_VALUE],
        step = config[CONF_STEP],)
    yield bluetoothex.register_bluetoothex_device(var, config)

    templ = yield cg.templatable(config[CONF_COMMAND_NUMBER], [(cg.float_.operator('const'), 'x')], cmd_t)
    cg.add(var.set_command_number(templ))
 
    state = config[CONF_STATE_NUMBER]
    if cg.is_template(state):
        templ = yield cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len'), (cg.float_, 'state')], cg.float_)
        cg.add(var.set_state_number(templ))
    else:
        args = yield state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION]
        cg.add(var.set_state_number(args))