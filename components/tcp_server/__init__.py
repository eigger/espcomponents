import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_DATA, CONF_TRIGGER_ID
from esphome.util import SimpleRegistry
from .const import CONF_RECV_BUFFER_SIZE, CONF_ON_WRITE, CONF_ON_READ, CONF_PORT

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["socket"]
tcp_server_ns = cg.esphome_ns.namespace('tcp_server')
TCP_ServerComponent = tcp_server_ns.class_('TCP_ServerComponent', cg.Component)
TCP_ServerWriteAction = tcp_server_ns.class_('TCP_ServerWriteAction', automation.Action)
vector_uint8 = cg.std_vector.template(cg.uint8)
uint16_const = cg.uint16.operator('const')
uint8_const = cg.uint8.operator('const')
uint8_ptr_const = uint8_const.operator('ptr')
WriteTrigger = tcp_server_ns.class_("WriteTrigger", automation.Trigger.template())
ReadTrigger = tcp_server_ns.class_("ReadTrigger", automation.Trigger.template())

MULTI_CONF = True

def validate_hex_data(value):
    if isinstance(value, str):
        return cv.Schema([cv.hex_uint8_t])([ord(char) for char in value])
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a string(ascii) or a list of bytes")

COMMAND_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): validate_hex_data
})

def shorthand_command_hex(value):
    value = validate_hex_data(value)
    return COMMAND_SCHEMA({CONF_DATA: value})

def command_hex_schema(value):
    if isinstance(value, dict):
        return COMMAND_SCHEMA(value)
    return shorthand_command_hex(value)

# TCP_Server Schema
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(TCP_ServerComponent),
    cv.Required(CONF_PORT): cv.port,
    cv.Optional(CONF_RECV_BUFFER_SIZE, default=256): cv.validate_bytes,
    cv.Optional(CONF_ON_WRITE): automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(WriteTrigger),
        }
    ),
    cv.Optional(CONF_ON_READ): automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ReadTrigger),
        }
    ),
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    cg.add_global(tcp_server_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    for conf in config.get(CONF_ON_WRITE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], conf)

    for conf in config.get(CONF_ON_READ, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], conf)

    if CONF_ON_WRITE in config:
        data = config[CONF_ON_WRITE]
        if cg.is_template(data):
            template_ = await cg.templatable(data, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.void)
            cg.add(var.set_on_write(template_))
            
    if CONF_ON_READ in config:
        data = config[CONF_ON_READ]
        if cg.is_template(data):
            template_ = await cg.templatable(data, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.void)
            cg.add(var.set_on_read(template_))

    if CONF_PORT in config:
        cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_recv_buffer_size(config[CONF_RECV_BUFFER_SIZE]))

HEX_SCHEMA_REGISTRY = SimpleRegistry()


@automation.register_action('tcp_server.write', TCP_ServerWriteAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(TCP_ServerComponent),
    cv.Required(CONF_DATA): cv.templatable(validate_hex_data)
}, key=CONF_DATA))

async def tcp_server_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]

    if cg.is_template(data):
        templ = await cg.templatable(data, args, vector_uint8)
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    return var
