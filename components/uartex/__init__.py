import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor
from esphome.components.text_sensor import register_text_sensor
from esphome import automation, pins, core
from esphome.const import CONF_ID, CONF_OFFSET, CONF_DATA, CONF_TRIGGER_ID, \
    CONF_INVERTED, CONF_VERSION, CONF_NAME, CONF_OPTIMISTIC, CONF_ICON, CONF_ENTITY_CATEGORY, ICON_NEW_BOX
from esphome.util import SimpleRegistry
from .const import CONF_RX_HEADER, CONF_RX_FOOTER, CONF_TX_HEADER, CONF_TX_FOOTER, \
    CONF_RX_CHECKSUM, CONF_TX_CHECKSUM, CONF_RX_CHECKSUM_2, CONF_TX_CHECKSUM_2, \
    CONF_UARTEX_ID, CONF_ERROR, CONF_LOG, CONF_ON_TX_TIMEOUT, \
    CONF_ACK, CONF_ON_WRITE, CONF_ON_READ, \
    CONF_STATE, CONF_MASK, \
    CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_ON, CONF_COMMAND_OFF, \
    CONF_COMMAND_UPDATE, CONF_RX_TIMEOUT, CONF_TX_TIMEOUT, CONF_TX_RETRY_CNT, \
    CONF_STATE_RESPONSE, CONF_LENGTH, CONF_PRECISION, CONF_RX_LENGTH, \
    CONF_TX_CTRL_PIN, CONF_TX_DELAY, CONF_DISABLED, CONF_ASCII, CONF_SIGNED, CONF_ENDIAN, CONF_DECODE

AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["uart"]
uartex_ns = cg.esphome_ns.namespace('uartex')
UARTExComponent = uartex_ns.class_('UARTExComponent', cg.Component, uart.UARTDevice)
UARTExDevice = uartex_ns.class_('UARTExDevice', cg.PollingComponent)
UARTExWriteAction = uartex_ns.class_('UARTExWriteAction', automation.Action)
cmd_t = uartex_ns.class_('cmd_t')
state_t = uartex_ns.class_('state_t')
state_num_t = uartex_ns.class_('state_num_t')
vector_uint8 = cg.std_vector.template(cg.uint8)
uint16_const = cg.uint16.operator('const')
uint8_const = cg.uint8.operator('const')
uint8_ptr_const = uint8_const.operator('ptr')
TxTimeoutTrigger = uartex_ns.class_("TxTimeoutTrigger", automation.Trigger.template())
WriteTrigger = uartex_ns.class_("WriteTrigger", automation.Trigger.template())
ReadTrigger = uartex_ns.class_("ReadTrigger", automation.Trigger.template())

MULTI_CONF = True
Checksum = uartex_ns.enum("CHECKSUM")
CHECKSUMS = {
    "NONE": Checksum.CHECKSUM_NONE,
    "XOR": Checksum.CHECKSUM_XOR,
    "ADD": Checksum.CHECKSUM_ADD,
    "XOR_NO_HEADER": Checksum.CHECKSUM_XOR_NO_HEADER,
    "ADD_NO_HEADER": Checksum.CHECKSUM_ADD_NO_HEADER,
    "XOR_ADD": Checksum.CHECKSUM_XOR_ADD,
}

def validate_checksum(value):
    if cg.is_template(value):
        return cv.returning_lambda(value)
    if isinstance(value, str):
        return cv.enum(CHECKSUMS, upper=True)(value)
    raise cv.Invalid("data type error")

Endian = uartex_ns.enum("ENDIAN")
ENDIANS = {
    "BIG": Endian.ENDIAN_BIG,
    "LITTLE": Endian.ENDIAN_LITTLE
}

def validate_endian(value):
    if isinstance(value, str):
        return cv.enum(ENDIANS, upper=True)(value)
    raise cv.Invalid("data type error")

Decode = uartex_ns.enum("DECODE")
DECODES = {
    "NONE": Decode.DECODE_NONE,
    "BCD": Decode.DECODE_BCD,
    "ASCII": Decode.DECODE_ASCII
}

def validate_decode(value):
    if isinstance(value, str):
        return cv.enum(DECODES, upper=True)(value)
    raise cv.Invalid("data type error")

def uartex_declare_type(value):
    return cv.use_id(UARTExComponent)(value)

def validate_hex_data(value):
    if isinstance(value, str):
        return cv.Schema([cv.hex_uint8_t])([ord(char) for char in value])
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a string(ascii) or a list of bytes")

STATE_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): validate_hex_data,
    cv.Optional(CONF_MASK, default=[]): validate_hex_data,
    cv.Optional(CONF_OFFSET, default=0): cv.int_range(min=0, max=128),
    cv.Optional(CONF_INVERTED, default=False): cv.boolean
})

def shorthand_state(value):
    value = validate_hex_data(value)
    return STATE_SCHEMA({CONF_DATA: value})

def state_schema(value):
    if isinstance(value, dict):
        return STATE_SCHEMA(value)
    return shorthand_state(value)

HEADER_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): validate_hex_data,
    cv.Optional(CONF_MASK, default=[]): validate_hex_data,
})

def shorthand_header(value):
    value = validate_hex_data(value)
    return HEADER_SCHEMA({CONF_DATA: value})

def header_schema(value):
    if isinstance(value, dict):
        return HEADER_SCHEMA(value)
    return shorthand_header(value)

COMMAND_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): validate_hex_data,
    cv.Optional(CONF_ACK, default=[]): validate_hex_data,
    cv.Optional(CONF_MASK, default=[]): validate_hex_data
})

def shorthand_command_hex(value):
    value = validate_hex_data(value)
    return COMMAND_SCHEMA({CONF_DATA: value, CONF_ACK: [], CONF_MASK: []})

def command_hex_schema(value):
    if isinstance(value, dict):
        return COMMAND_SCHEMA(value)
    return shorthand_command_hex(value)

# UARTEx Schema
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(UARTExComponent),
    cv.Optional(CONF_RX_TIMEOUT, default="10ms"): cv.All(
        cv.positive_time_period_milliseconds,
        cv.Range(max=core.TimePeriod(milliseconds=2000)),
    ),
    cv.Optional(CONF_TX_DELAY, default="50ms"): cv.All(
        cv.positive_time_period_milliseconds,
        cv.Range(max=core.TimePeriod(milliseconds=2000)),
    ),
    cv.Optional(CONF_TX_TIMEOUT, default="50ms"): cv.All(
        cv.positive_time_period_milliseconds,
        cv.Range(max=core.TimePeriod(milliseconds=2000)),
    ),
    cv.Optional(CONF_TX_RETRY_CNT, default=3): cv.int_range(min=1, max=10),
    cv.Optional(CONF_ON_TX_TIMEOUT): automation.validate_automation(
        {
            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(TxTimeoutTrigger),
        }
    ),    
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
    cv.Optional(CONF_RX_LENGTH): cv.int_range(min=1, max=256),
    cv.Optional(CONF_TX_CTRL_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RX_HEADER): header_schema,
    cv.Optional(CONF_RX_FOOTER): validate_hex_data,
    cv.Optional(CONF_TX_HEADER): validate_hex_data,
    cv.Optional(CONF_TX_FOOTER): validate_hex_data,
    cv.Optional(CONF_RX_CHECKSUM): validate_checksum,
    cv.Optional(CONF_TX_CHECKSUM): validate_checksum,
    cv.Optional(CONF_RX_CHECKSUM_2): validate_checksum,
    cv.Optional(CONF_TX_CHECKSUM_2): validate_checksum,
    cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(text_sensor.TextSensor).extend(
    {
        cv.Optional(CONF_NAME, default="Version"): cv._validate_entity_name,
        cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
        cv.Optional(CONF_DISABLED, default=False): cv.boolean,
    }),
    cv.Optional(CONF_ERROR): text_sensor.text_sensor_schema(text_sensor.TextSensor).extend(
    {
        cv.Optional(CONF_NAME, default="Error"): cv._validate_entity_name,
        cv.Optional(CONF_ICON, default="mdi:alert-circle"): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
        cv.Optional(CONF_DISABLED, default=False): cv.boolean,
    }),
    cv.Optional(CONF_LOG): text_sensor.text_sensor_schema(text_sensor.TextSensor).extend(
    {
        cv.Optional(CONF_NAME, default="Log"): cv._validate_entity_name,
        cv.Optional(CONF_ICON, default="mdi:math-log"): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
        cv.Optional(CONF_DISABLED, default=False): cv.boolean,
        cv.Optional(CONF_ASCII, default=False): cv.boolean,
    }),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA), cv.has_at_most_one_key(CONF_RX_CHECKSUM, CONF_RX_CHECKSUM_2), cv.has_at_most_one_key(CONF_TX_CHECKSUM, CONF_TX_CHECKSUM_2))

async def to_code(config):
    cg.add_global(uartex_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CONF_VERSION in config:
        if not config[CONF_VERSION][CONF_DISABLED]:
            sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
            await register_text_sensor(sens, config[CONF_VERSION])
            cg.add(var.set_version(sens))

    if CONF_ERROR in config:
        if not config[CONF_ERROR][CONF_DISABLED]:
            sens = cg.new_Pvariable(config[CONF_ERROR][CONF_ID])
            await register_text_sensor(sens, config[CONF_ERROR])
            cg.add(var.set_error(sens))

    if CONF_LOG in config:
        if not config[CONF_LOG][CONF_DISABLED]:
            sens = cg.new_Pvariable(config[CONF_LOG][CONF_ID])
            await register_text_sensor(sens, config[CONF_LOG])
            cg.add(var.set_log(sens))
            cg.add(var.set_log_ascii(config[CONF_LOG][CONF_ASCII]))

    if CONF_RX_TIMEOUT in config:
        cg.add(var.set_rx_timeout(config[CONF_RX_TIMEOUT]))

    if CONF_TX_DELAY in config:
        cg.add(var.set_tx_delay(config[CONF_TX_DELAY]))

    if CONF_TX_TIMEOUT in config:
        cg.add(var.set_tx_timeout(config[CONF_TX_TIMEOUT]))

    if CONF_TX_RETRY_CNT in config:
        cg.add(var.set_tx_retry_cnt(config[CONF_TX_RETRY_CNT]))
    
    for conf in config.get(CONF_ON_TX_TIMEOUT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_WRITE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], conf)

    for conf in config.get(CONF_ON_READ, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], conf)

    if CONF_RX_LENGTH in config:
        cg.add(var.set_rx_length(config[CONF_RX_LENGTH]))

    if CONF_TX_CTRL_PIN in config:
        tx_ctrl_pin = await cg.gpio_pin_expression(config[CONF_TX_CTRL_PIN])
        cg.add(var.set_tx_ctrl_pin(tx_ctrl_pin))

    if CONF_RX_HEADER in config:
        header = header_hex_expression(config[CONF_RX_HEADER])
        cg.add(var.set_rx_header(header))

    if CONF_RX_FOOTER in config:
        cg.add(var.set_rx_footer(config[CONF_RX_FOOTER]))

    if CONF_TX_HEADER in config:
        cg.add(var.set_tx_header(config[CONF_TX_HEADER]))

    if CONF_TX_FOOTER in config:
        cg.add(var.set_tx_footer(config[CONF_TX_FOOTER]))

    if CONF_RX_CHECKSUM in config:
        data = config[CONF_RX_CHECKSUM]
        if cg.is_template(data):
            template_ = await cg.templatable(data, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.uint8)
            cg.add(var.set_rx_checksum(template_))
        else:
            cg.add(var.set_rx_checksum(data))

    if CONF_TX_CHECKSUM in config:
        data = config[CONF_TX_CHECKSUM]
        if cg.is_template(data):
            template_ = await cg.templatable(data, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.uint8)
            cg.add(var.set_tx_checksum(template_))
        else:
            cg.add(var.set_tx_checksum(data))

    if CONF_RX_CHECKSUM_2 in config:
        data = config[CONF_RX_CHECKSUM_2]
        if cg.is_template(data):
            template_ = await cg.templatable(data, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], vector_uint8)
            cg.add(var.set_rx_checksum_2(template_))
        else:
            cg.add(var.set_rx_checksum_2(data))

    if CONF_TX_CHECKSUM_2 in config:
        data = config[CONF_TX_CHECKSUM_2]
        if cg.is_template(data):
            template_ = await cg.templatable(data, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], vector_uint8)
            cg.add(var.set_tx_checksum_2(template_))
        else:
            cg.add(var.set_tx_checksum_2(data))

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

# A schema to use for all UARTEx devices, all UARTEx integrations must extend this!
UARTEX_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_UARTEX_ID): uartex_declare_type,
    cv.Optional(CONF_STATE): state_schema,
    cv.Optional(CONF_STATE_ON): state_schema,
    cv.Optional(CONF_STATE_OFF): state_schema,
    cv.Optional(CONF_COMMAND_ON): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_UPDATE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_STATE_RESPONSE): state_schema,
    cv.Optional(CONF_OPTIMISTIC, default=False): cv.boolean,
}).extend(cv.polling_component_schema('60s'))

STATE_NUM_SCHEMA = cv.Schema({
    cv.Required(CONF_OFFSET): cv.int_range(min=0, max=128),
    cv.Optional(CONF_LENGTH, default=1): cv.int_range(min=1, max=4),
    cv.Optional(CONF_PRECISION, default=0): cv.int_range(min=0, max=5),
    cv.Optional(CONF_SIGNED, default=True): cv.boolean,
    cv.Optional(CONF_ENDIAN, default="big"): validate_endian,
    cv.Optional(CONF_DECODE, default="none"): validate_decode,
})

def state_num_schema(value):
    return STATE_NUM_SCHEMA(value)

HEX_SCHEMA_REGISTRY = SimpleRegistry()

async def register_uartex_device(var, config):
    paren = await cg.get_variable(config[CONF_UARTEX_ID])
    cg.add(paren.register_device(var))

    if CONF_STATE in config:
        state = state_hex_expression(config[CONF_STATE])
        cg.add(var.set_state(CONF_STATE, state))

    if CONF_STATE_ON in config:
        state = state_hex_expression(config[CONF_STATE_ON])
        cg.add(var.set_state(CONF_STATE_ON, state))

    if CONF_STATE_OFF in config:
        state = state_hex_expression(config[CONF_STATE_OFF])
        cg.add(var.set_state(CONF_STATE_OFF, state))

    if CONF_COMMAND_ON in config:
        command = await command_expression(config[CONF_COMMAND_ON])
        cg.add(var.set_command(CONF_COMMAND_ON, command))

    if CONF_COMMAND_OFF in config:
        command = await command_expression(config[CONF_COMMAND_OFF])
        cg.add(var.set_command(CONF_COMMAND_OFF, command))

    if CONF_COMMAND_UPDATE in config:
        command = await command_expression(config[CONF_COMMAND_UPDATE])
        cg.add(var.set_command(CONF_COMMAND_UPDATE, command))
    
    if CONF_STATE_RESPONSE in config:
        state = state_hex_expression(config[CONF_STATE_RESPONSE])
        cg.add(var.set_state(CONF_STATE_RESPONSE, state))
    
    if CONF_OPTIMISTIC in config:
        cg.add(var.set_optimistic(config[CONF_OPTIMISTIC]))

def header_hex_expression(conf):
    if conf is None:
        return
    data = conf[CONF_DATA]
    mask = conf[CONF_MASK]
    return data, mask

def state_hex_expression(conf):
    if conf is None:
        return
    data = conf[CONF_DATA]
    mask = conf[CONF_MASK]
    offset = conf[CONF_OFFSET]
    inverted = conf[CONF_INVERTED]
    return state_t(data, mask, offset, inverted)

def state_num_hex_expression(conf):
    if conf is None:
        return
    offset = conf[CONF_OFFSET]
    length = conf[CONF_LENGTH]
    precision = conf[CONF_PRECISION]
    signed = conf[CONF_SIGNED]
    endian = conf[CONF_ENDIAN]
    decode = conf[CONF_DECODE]
    return state_num_t(offset, length, precision, signed, endian, decode)

async def state_num_expression(conf):
    if cg.is_template(conf):
        return await cg.templatable(conf, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
    else:
        return state_num_hex_expression(conf)
    
async def state_string_expression(conf):
    if cg.is_template(conf):
        return await cg.templatable(conf, [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.std_string)
    
def command_hex_expression(conf):
    if conf is None:
        return
    data = conf[CONF_DATA]
    ack = conf[CONF_ACK]
    mask = conf[CONF_MASK]
    return cmd_t(data, ack, mask)

async def command_expression(conf):
    if cg.is_template(conf):
        return await cg.templatable(conf, [], cmd_t)
    else:
        return command_hex_expression(conf)

async def command_float_expression(conf):
    if cg.is_template(conf):
        return await cg.templatable(conf, [(cg.float_.operator('const'), 'x')], cmd_t)
    else:
        return command_hex_expression(conf)
    
async def command_string_expression(conf):
    if cg.is_template(conf):
        return await cg.templatable(conf, [(cg.std_string.operator('const'), 'str')], cmd_t)
    else:
        return command_hex_expression(conf)

@automation.register_action('uartex.write', UARTExWriteAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(UARTExComponent),
    cv.Required(CONF_DATA): cv.templatable(validate_hex_data),
    cv.Optional(CONF_ACK, default=[]): validate_hex_data,
    cv.Optional(CONF_MASK, default=[]): validate_hex_data
}, key=CONF_DATA))

async def uartex_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]

    if cg.is_template(data):
        templ = await cg.templatable(data, args, cmd_t)
        cg.add(var.set_data_template(templ))
    else:
        cmd = command_hex_expression(config)
        cg.add(var.set_data_static(cmd))
    return var
