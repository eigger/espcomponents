import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, uartex
from esphome.const import CONF_ID, CONF_MIN_VALUE, CONF_MAX_VALUE, CONF_STEP, CONF_OFFSET
from .. import uartex_ns, cmd_t, uint8_ptr_const, uint16_const, \
    state_num_schema
from ..const import CONF_COMMAND_NUMBER, CONF_COMMAND_OFF, CONF_STATE_NUMBER, CONF_STATE_OFF, \
    CONF_COMMAND_ON, CONF_STATE_ON, CONF_LENGTH, CONF_PRECISION, CONF_SIGNED, CONF_ENDIAN, CONF_BCD

DEPENDENCIES = ['uartex']
UARTExNumber = uartex_ns.class_('UARTExNumber', number.Number, cg.Component)

CONFIG_SCHEMA = cv.All(number.NUMBER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExNumber),
    cv.Required(CONF_MIN_VALUE): cv.float_,
    cv.Required(CONF_MAX_VALUE): cv.float_,
    cv.Required(CONF_STEP): cv.float_,
    cv.Optional(CONF_STATE_NUMBER): cv.templatable(state_num_schema),
    cv.Optional(CONF_COMMAND_NUMBER): cv.returning_lambda,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Number do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Number do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Number do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Number do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_STATE_NUMBER, CONF_COMMAND_NUMBER))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(            
        var,
        config,
        min_value = config[CONF_MIN_VALUE],
        max_value = config[CONF_MAX_VALUE],
        step = config[CONF_STEP],)
    await uartex.register_uartex_device(var, config)

    if CONF_COMMAND_NUMBER in config:
        templ = await cg.templatable(config[CONF_COMMAND_NUMBER], [(cg.float_.operator('const'), 'x')], cmd_t)
        cg.add(var.set_command(CONF_COMMAND_NUMBER, templ))
        
    if CONF_STATE_NUMBER in config:
        state = config[CONF_STATE_NUMBER]
        if cg.is_template(state):
            templ = await cg.templatable(state, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
            cg.add(var.set_state(CONF_STATE_NUMBER, templ))
        else:
            args = state[CONF_OFFSET], state[CONF_LENGTH], state[CONF_PRECISION], state[CONF_SIGNED], state[CONF_ENDIAN], state[CONF_BCD]
            cg.add(var.set_state(CONF_STATE_NUMBER, args))