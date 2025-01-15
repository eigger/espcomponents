import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor, uartex
from esphome.const import CONF_ID, CONF_LAMBDA
from esphome.components.uartex import uint8_ptr_const, uint16_const, \
    state_schema, _uartex_declare_type
from esphome.components.uartex.const import CONF_STATE, CONF_UARTEX_ID

DEPENDENCIES = ['uartex']
at_http_ns = cg.esphome_ns.namespace('at_http')
ATHttpTextSensor = at_http_ns.class_('ATHttpTextSensor', text_sensor.TextSensor, cg.PollingComponent)
HttpMethod = at_http_ns.enum("HTTP_METHOD")
HTTP_METHOD = {
    "HEAD": HttpMethod.HEAD,
    "GET": HttpMethod.GET,
    "POST": HttpMethod.POST,
    "PUT": HttpMethod.PUT,
    "DELETE": HttpMethod.DELETE,
}

HttpContent = at_http_ns.enum("HTTP_CONTENT")
HTTP_CONTENT = {
    "APPLICATION_X_WWW_FORM_URLENCODED": HttpContent.APPLICATION_X_WWW_FORM_URLENCODED,
    "APPLICATION_JSON": HttpContent.APPLICATION_JSON,
    "MULTIPART_FORM_DATA": HttpContent.MULTIPART_FORM_DATA,
    "TEXT_XML": HttpContent.TEXT_XML,
}

def validate_method(value):
    if isinstance(value, str):
        return cv.enum(HTTP_METHOD, upper=True)(value)
    raise cv.Invalid("data type error")

def validate_content(value):
    if isinstance(value, str):
        return cv.enum(HTTP_CONTENT, upper=True)(value)
    raise cv.Invalid("data type error")

CONF_HTTP_METHOD = 'http_method'
CONF_HTTP_CONTENT = 'http_content'
CONF_HTTP_URL = 'http_url'
CONF_HTTP_PAYLOAD = 'http_payload'

CONFIG_SCHEMA = cv.All(text_sensor.TEXT_SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ATHttpTextSensor),
    cv.GenerateID(CONF_UARTEX_ID): _uartex_declare_type,
    cv.Optional(CONF_STATE, default=[]): state_schema,
    cv.Required(CONF_LAMBDA): cv.returning_lambda,
    cv.Required(CONF_HTTP_METHOD): cv.validate_method,
    cv.Required(CONF_HTTP_CONTENT): cv.validate_content,
    cv.Required(CONF_HTTP_URL): cv.str,
    cv.Required(CONF_HTTP_PAYLOAD): cv.str,
}).extend(cv.polling_component_schema('60s')))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    await uartex.register_uartex_device(var, config)
    
    if CONF_LAMBDA in config:
        template_ = await cg.templatable(config[CONF_LAMBDA], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.const_char_ptr)
        cg.add(var.set_state(CONF_LAMBDA, template_))

    if CONF_HTTP_METHOD in config:
        cg.add(var.set_http_method(config[CONF_HTTP_METHOD]))
    
    if CONF_HTTP_CONTENT in config:
        cg.add(var.set_http_content(config[CONF_HTTP_CONTENT]))

    if CONF_HTTP_URL in config:
        cg.add(var.set_http_url(config[CONF_HTTP_URL]))

    if CONF_HTTP_PAYLOAD in config:
        cg.add(var.set_http_payload(config[CONF_HTTP_PAYLOAD]))