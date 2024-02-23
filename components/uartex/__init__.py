import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor
from esphome.components.text_sensor import register_text_sensor
from esphome import automation, pins, core
from esphome.const import CONF_ID, CONF_OFFSET, CONF_DATA, \
    CONF_INVERTED, CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, ICON_NEW_BOX
from esphome.util import SimpleRegistry
from .const import CONF_RX_HEADER, CONF_RX_FOOTER, CONF_TX_HEADER, CONF_TX_FOOTER, \
    CONF_RX_CHECKSUM, CONF_TX_CHECKSUM, CONF_RX_CHECKSUM_2, CONF_TX_CHECKSUM_2, \
    CONF_UARTEX_ID, CONF_ERROR, \
    CONF_ACK, CONF_ON_WRITE, CONF_ON_READ, \
    CONF_STATE, CONF_MASK, \
    CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_ON, CONF_COMMAND_OFF, \
    CONF_COMMAND_UPDATE, CONF_RX_TIMEOUT, CONF_TX_TIMEOUT, CONF_TX_RETRY_CNT, \
    CONF_STATE_RESPONSE, CONF_LENGTH, CONF_PRECISION, \
    CONF_TX_CTRL_PIN, CONF_TX_DELAY

AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["uart"]
uartex_ns = cg.esphome_ns.namespace('uartex')
UARTExComponent = uartex_ns.class_('UARTExComponent', cg.Component, uart.UARTDevice)
UARTExWriteAction = uartex_ns.class_('UARTExWriteAction', automation.Action)
cmd_t = uartex_ns.class_('cmd_t')
vector_uint8 = cg.std_vector.template(cg.uint8)
uint16_const = cg.uint16.operator('const')
uint8_const = cg.uint8.operator('const')
uint8_ptr_const = uint8_const.operator('ptr')

MULTI_CONF = True
Checksum = uartex_ns.enum("CHECKSUM")
CHECKSUMS = {
    "NONE": Checksum.CHECKSUM_NONE,
    "XOR": Checksum.CHECKSUM_XOR,
    "ADD": Checksum.CHECKSUM_ADD,
}

def _uartex_declare_type(value):
    return cv.use_id(UARTExComponent)(value)

def validate_hex_data(value):
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a list of bytes")

def validate_checksum(value):
    if cg.is_template(value):
        return cv.returning_lambda(value)
    if isinstance(value, str):
        return cv.enum(CHECKSUMS, upper=True)(value)
    raise cv.Invalid("data type error")

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

COMMAND_HEX_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): validate_hex_data,
    cv.Optional(CONF_ACK, default=[]): validate_hex_data
})

def shorthand_command_hex(value):
    value = validate_hex_data(value)
    return COMMAND_HEX_SCHEMA({CONF_DATA: value, CONF_ACK: []})

def command_hex_schema(value):
    if isinstance(value, dict):
        return COMMAND_HEX_SCHEMA(value)
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
    cv.Optional(CONF_TX_CTRL_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RX_HEADER): validate_hex_data,
    cv.Optional(CONF_RX_FOOTER): validate_hex_data,
    cv.Optional(CONF_TX_HEADER): validate_hex_data,
    cv.Optional(CONF_TX_FOOTER): validate_hex_data,
    cv.Optional(CONF_RX_CHECKSUM): validate_checksum,
    cv.Optional(CONF_TX_CHECKSUM): validate_checksum,
    cv.Optional(CONF_RX_CHECKSUM_2): validate_checksum,
    cv.Optional(CONF_TX_CHECKSUM_2): validate_checksum,
    cv.Optional(CONF_ON_WRITE): cv.lambda_,
    cv.Optional(CONF_ON_READ): cv.lambda_,
    cv.Optional(CONF_VERSION, default={CONF_NAME: "UartEX Version"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
    cv.Optional(CONF_ERROR, default={CONF_NAME: "Error"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default="mdi:alert-circle"): cv.icon,
        cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
    }),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA), cv.has_at_most_one_key(CONF_RX_CHECKSUM, CONF_RX_CHECKSUM_2), cv.has_at_most_one_key(CONF_TX_CHECKSUM, CONF_TX_CHECKSUM_2))

async def to_code(config):
    cg.add_global(uartex_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if CONF_VERSION in config:
        sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
        await register_text_sensor(sens, config[CONF_VERSION])
        cg.add(var.set_version(sens))
    if CONF_ERROR in config:
        sens = cg.new_Pvariable(config[CONF_ERROR][CONF_ID])
        await register_text_sensor(sens, config[CONF_ERROR])
        cg.add(var.set_error(sens))
    if CONF_RX_TIMEOUT in config:
        cg.add(var.set_rx_timeout(config[CONF_RX_TIMEOUT]))
    if CONF_TX_DELAY in config:
        cg.add(var.set_tx_delay(config[CONF_TX_DELAY]))
    if CONF_TX_TIMEOUT in config:
        cg.add(var.set_tx_timeout(config[CONF_TX_TIMEOUT]))
    if CONF_TX_RETRY_CNT in config:
        cg.add(var.set_tx_retry_cnt(config[CONF_TX_RETRY_CNT]))
    if CONF_TX_CTRL_PIN in config:
        tx_ctrl_pin = await cg.gpio_pin_expression(config[CONF_TX_CTRL_PIN])
        cg.add(var.set_tx_ctrl_pin(tx_ctrl_pin))
    if CONF_RX_HEADER in config:
        cg.add(var.set_rx_header(config[CONF_RX_HEADER]))
    if CONF_RX_FOOTER in config:
        cg.add(var.set_rx_footer(config[CONF_RX_FOOTER]))
    if CONF_TX_HEADER in config:
        cg.add(var.set_tx_header(config[CONF_TX_HEADER]))
    if CONF_TX_FOOTER in config:
        cg.add(var.set_tx_footer(config[CONF_TX_FOOTER]))
    if CONF_RX_CHECKSUM in config:
        data = config[CONF_RX_CHECKSUM]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len')],
                                                return_type=cg.uint8)
            cg.add(var.set_rx_checksum_lambda(template_))
        else:
            cg.add(var.set_rx_checksum(data))
    if CONF_TX_CHECKSUM in config:
        data = config[CONF_TX_CHECKSUM]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len')],
                                                return_type=cg.uint8)
            cg.add(var.set_tx_checksum_lambda(template_))
        else:
            cg.add(var.set_tx_checksum(data))
    if CONF_RX_CHECKSUM_2 in config:
        data = config[CONF_RX_CHECKSUM_2]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len')],
                                                return_type=vector_uint8)
            cg.add(var.set_rx_checksum_2_lambda(template_))
        else:
            cg.add(var.set_rx_checksum_2(data))
    if CONF_TX_CHECKSUM_2 in config:
        data = config[CONF_TX_CHECKSUM_2]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len')],
                                                return_type=vector_uint8)
            cg.add(var.set_tx_checksum_2_lambda(template_))
        else:
            cg.add(var.set_tx_checksum_2(data))
    if CONF_ON_WRITE in config:
        data = config[CONF_ON_WRITE]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len')],
                                                return_type=cg.void)
        cg.add(var.set_on_write(template_))
    if CONF_ON_READ in config:
        data = config[CONF_ON_READ]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len')],
                                                return_type=cg.void)
        cg.add(var.set_on_read(template_))

# A schema to use for all UARTEx devices, all UARTEx integrations must extend this!
UARTEX_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_UARTEX_ID): _uartex_declare_type,
    cv.Required(CONF_STATE): state_schema,
    cv.Required(CONF_STATE_ON): state_schema,
    cv.Required(CONF_STATE_OFF): state_schema,
    cv.Required(CONF_COMMAND_ON): cv.templatable(command_hex_schema),
    cv.Required(CONF_COMMAND_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_UPDATE): command_hex_schema,
    cv.Optional(CONF_STATE_RESPONSE): state_schema,
}).extend(cv.polling_component_schema('60s'))

STATE_NUM_SCHEMA = cv.Schema({
    cv.Required(CONF_OFFSET): cv.int_range(min=0, max=128),
    cv.Optional(CONF_LENGTH, default=1): cv.int_range(min=1, max=4),
    cv.Optional(CONF_PRECISION, default=0): cv.int_range(min=0, max=5)
})


HEX_SCHEMA_REGISTRY = SimpleRegistry()

async def register_uartex_device(var, config):
    paren = await cg.get_variable(config[CONF_UARTEX_ID])
    cg.add(paren.register_device(var))

    if CONF_STATE in config:
        state = state_hex_expression(config[CONF_STATE])
        cg.add(var.set_state(state))

    if CONF_STATE_ON in config:
        state_on = state_hex_expression(config[CONF_STATE_ON])
        cg.add(var.set_state_on(state_on))

    if CONF_STATE_OFF in config:
        state_off = state_hex_expression(config[CONF_STATE_OFF])
        cg.add(var.set_state_off(state_off))

    if CONF_COMMAND_ON in config:
        data = config[CONF_COMMAND_ON]
        if cg.is_template(data):
            #command_on = await cg.templatable(data, [(uint8_ptr_const, 'state'), (uint16_const, 'len')], cmd_t)
            command_on = await cg.process_lambda(data, [(uint8_ptr_const, 'state'), (uint16_const, 'len')], return_type=cmd_t)
            cg.add(var.set_command_on(command_on))
        else:
            command_on = command_hex_expression(config[CONF_COMMAND_ON])
            cg.add(var.set_command_on(command_on))

    if CONF_COMMAND_OFF in config:
        data = config[CONF_COMMAND_OFF]
        if cg.is_template(data):
            #command_off = await cg.templatable(data, [(uint8_ptr_const, 'state'), (uint16_const, 'len')], cmd_t)
            command_off = await cg.process_lambda(data, [(uint8_ptr_const, 'state'), (uint16_const, 'len')], return_type=cmd_t)
            cg.add(var.set_command_off(command_off))
        else:
            command_off = command_hex_expression(config[CONF_COMMAND_OFF])
            cg.add(var.set_command_off(command_off))

    if CONF_COMMAND_UPDATE in config:
        command_update = command_hex_expression(config[CONF_COMMAND_UPDATE])
        cg.add(var.set_command_update(command_update))
    
    if CONF_STATE_RESPONSE in config:
        state_response = state_hex_expression(config[CONF_STATE_RESPONSE])
        cg.add(var.set_state_response(state_response))


def state_hex_expression(conf):
    if conf is None:
        raise cv.Invalid("data type error {conf}")
        return
    data = conf[CONF_DATA]
    mask = conf[CONF_MASK]
    inverted = conf[CONF_INVERTED]
    offset = conf[CONF_OFFSET]
    offset, inverted, data, mask


def command_hex_expression(conf):
    if conf is None:
        raise cv.Invalid("data type error {conf}")
        return
    data = conf[CONF_DATA]
    if CONF_ACK in conf:
        ack = conf[CONF_ACK]
        data, ack
    else:
        data

@automation.register_action('uartex.write', UARTExWriteAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(UARTExComponent),
    cv.Required(CONF_DATA): cv.templatable(validate_hex_data),
    cv.Optional(CONF_ACK, default=[]): validate_hex_data
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
