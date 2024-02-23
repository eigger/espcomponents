import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import text, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from .. import uartex_ns, UARTExComponent, uint8_ptr_const, uint16_const, cmd_t, \
    state_schema, command_hex_schema, _uartex_declare_type
from ..const import CONF_UARTEX_ID, CONF_STATE, CONF_COMMAND_UPDATE, CONF_STATE_TEXT, CONF_COMMAND_TEXT

DEPENDENCIES = ['uartex']
UARTExText = uartex_ns.class_('UARTExText', text.Text, cg.PollingComponent)

CONFIG_SCHEMA = cv.All(text.TEXT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExText),
    cv.GenerateID(CONF_UARTEX_ID): _uartex_declare_type,
    # cv.Optional(CONF_STATE): state_schema,
    # cv.Optional(CONF_COMMAND_UPDATE): command_hex_schema,
    # cv.Optional(CONF_STATE_TEXT): cv.returning_lambda,
    cv.Required(CONF_COMMAND_TEXT): cv.returning_lambda,
}).extend(cv.polling_component_schema('60s')))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    # if CONF_STATE_TEXT in config:
    #     template_ = yawaitield cg.process_lambda(config[CONF_STATE_TEXT], [(uint8_ptr_const, 'data'),
    #                                                               (uint16_const, 'len')],
    #                                         return_type=cg.optional.template(cg.std_string))
    #     cg.add(var.set_template(template_))
    templ = await cg.templatable(config[CONF_COMMAND_TEXT], [(cg.std_string.operator('const'), 'str')], cmd_t)
    cg.add(var.set_command_text(templ))

    await text.register_text(var, config)
