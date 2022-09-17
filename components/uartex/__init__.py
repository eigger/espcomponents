import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, text_sensor
from esphome import automation, pins
from esphome.const import CONF_ID, CONF_OFFSET, CONF_DATA, \
    CONF_DEVICE, CONF_INVERTED, CONF_VERSION, CONF_NAME, CONF_ICON, ICON_NEW_BOX
from esphome.core import coroutine
from esphome.util import SimpleRegistry
from .const import CONF_RX_PREFIX, CONF_RX_SUFFIX, CONF_TX_PREFIX, CONF_TX_SUFFIX, \
    CONF_RX_CHECKSUM, CONF_TX_CHECKSUM, CONF_RX_CHECKSUM_2, CONF_TX_CHECKSUM_2, \
    CONF_UARTEX_ID, \
    CONF_ACK, \
    CONF_SUB_DEVICE, \
    CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_ON, CONF_COMMAND_OFF, \
    CONF_COMMAND_STATE, CONF_RX_WAIT, CONF_TX_WAIT, CONF_TX_RETRY_CNT, \
    CONF_STATE_RESPONSE, CONF_LENGTH, CONF_PRECISION, CONF_AND_OPERATOR, \
    CONF_CTRL_PIN, CONF_STATUS_PIN, CONF_TX_INTERVAL

_LOGGER = logging.getLogger(__name__)
DEPENDENCIES = ["uart", "text_sensor"]
uartex_ns = cg.esphome_ns.namespace('uartex')
UARTExComponent = uartex_ns.class_('UARTExComponent', cg.Component, uart.UARTDevice, text_sensor.TextSensor)
UARTExWriteAction = uartex_ns.class_('UARTExWriteAction', automation.Action)
cmd_t = uartex_ns.class_('cmd_t')
uint16_const = cg.uint16.operator('const')
uint8_const = cg.uint8.operator('const')
uint8_ptr_const = uint8_const.operator('ptr')

MULTI_CONF = False

Checksum = uartex_ns.enum("Checksum")
CHECKSUMS = {
    "NONE": Checksum.CHECKSUM_NONE,
    "XOR": Checksum.CHECKSUM_XOR,
    "ADD": Checksum.CHECKSUM_ADD,
}

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
    cv.Optional(CONF_OFFSET, default=0): cv.int_range(min=0, max=128),
    cv.Optional(CONF_AND_OPERATOR, default=False): cv.boolean,
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
    cv.Optional(CONF_RX_WAIT, default=10): cv.int_range(min=1, max=2000),
    cv.Optional(CONF_TX_INTERVAL): cv.int_range(min=1, max=2000),
    cv.Optional(CONF_TX_WAIT): cv.int_range(min=1, max=2000),
    cv.Optional(CONF_TX_RETRY_CNT): cv.int_range(min=1, max=10),
    cv.Optional(CONF_CTRL_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_STATUS_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RX_PREFIX): validate_hex_data,
    cv.Optional(CONF_RX_SUFFIX): validate_hex_data,
    cv.Optional(CONF_TX_PREFIX): validate_hex_data,
    cv.Optional(CONF_TX_SUFFIX): validate_hex_data,
    cv.Optional(CONF_RX_CHECKSUM, default="none"): validate_checksum,
    cv.Optional(CONF_TX_CHECKSUM, default="none"): validate_checksum,
    cv.Optional(CONF_RX_CHECKSUM_2, default="none"): validate_checksum,
    cv.Optional(CONF_TX_CHECKSUM_2, default="none"): validate_checksum,
    cv.Optional(CONF_VERSION, default={CONF_NAME: "UartEX Version"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
        cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
    }),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA).extend(text_sensor.TEXT_SENSOR_SCHEMA)
)

async def to_code(config):
    cg.add_global(uartex_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    await text_sensor.register_text_sensor(var, config)
    # if CONF_VERSION in config:
    #     sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
    #     await text_sensor.register_text_sensor(sens, config[CONF_VERSION])
    #     cg.add(var.set_version(sens))

    if CONF_RX_WAIT in config:
        cg.add(var.set_rx_wait(config[CONF_RX_WAIT]))
    if CONF_TX_INTERVAL in config:
        cg.add(var.set_tx_interval(config[CONF_TX_INTERVAL]))
    if CONF_TX_WAIT in config:
        cg.add(var.set_tx_wait(config[CONF_TX_WAIT]))
    if CONF_TX_RETRY_CNT in config:
        cg.add(var.set_tx_retry_cnt(config[CONF_TX_RETRY_CNT]))
    if CONF_CTRL_PIN in config:
        ctrl_pin = await cg.gpio_pin_expression(config[CONF_CTRL_PIN])
        cg.add(var.set_ctrl_pin(ctrl_pin))
    if CONF_STATUS_PIN in config:
        status_pin = await cg.gpio_pin_expression(config[CONF_STATUS_PIN])
        cg.add(var.set_status_pin(status_pin))
    if CONF_RX_PREFIX in config:
        cg.add(var.set_rx_prefix(config[CONF_RX_PREFIX]))
    if CONF_RX_SUFFIX in config:
        cg.add(var.set_rx_suffix(config[CONF_RX_SUFFIX]))
    if CONF_TX_PREFIX in config:
        cg.add(var.set_tx_prefix(config[CONF_TX_PREFIX]))
    if CONF_TX_SUFFIX in config:
        cg.add(var.set_tx_suffix(config[CONF_TX_SUFFIX]))
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
    if CONF_RX_CHECKSUM_2 in config:
        data = config[CONF_RX_CHECKSUM_2]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len'),
                                                 (uint8_const, 'checksum')],
                                                return_type=cg.uint8)
            cg.add(var.set_rx_checksum_2_lambda(template_))
        else:
            cg.add(var.set_rx_checksum_2(data))
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
    if CONF_TX_CHECKSUM_2 in config:
        data = config[CONF_TX_CHECKSUM_2]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (uint16_const, 'len'),
                                                 (uint8_const, 'checksum')],
                                                return_type=cg.uint8)
            cg.add(var.set_tx_checksum_2_lambda(template_))
        else:
            cg.add(var.set_tx_checksum_2(data))
# A schema to use for all UARTEx devices, all UARTEx integrations must extend this!
UARTEX_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_UARTEX_ID): cv.use_id(UARTExComponent),
    cv.Required(CONF_DEVICE): state_schema,
    cv.Optional(CONF_SUB_DEVICE): state_schema,
    cv.Required(CONF_STATE_ON): state_schema,
    cv.Required(CONF_STATE_OFF): state_schema,
    cv.Required(CONF_COMMAND_ON): cv.templatable(command_hex_schema),
    cv.Required(CONF_COMMAND_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_STATE): command_hex_schema,
    cv.Optional(CONF_STATE_RESPONSE): state_schema,
}).extend(cv.polling_component_schema('60s'))

STATE_NUM_SCHEMA = cv.Schema({
    cv.Required(CONF_OFFSET): cv.int_range(min=0, max=128),
    cv.Optional(CONF_LENGTH, default=1): cv.int_range(min=1, max=4),
    cv.Optional(CONF_PRECISION, default=0): cv.int_range(min=0, max=5)
})


HEX_SCHEMA_REGISTRY = SimpleRegistry()

@coroutine
def register_uartex_device(var, config):
    paren = yield cg.get_variable(config[CONF_UARTEX_ID])
    cg.add(paren.register_device(var))
    yield var

    if CONF_DEVICE in config:
        device = yield state_hex_expression(config[CONF_DEVICE])
        cg.add(var.set_device(device))

    if CONF_SUB_DEVICE in config:
        sub_device = yield state_hex_expression(config[CONF_SUB_DEVICE])
        cg.add(var.set_sub_device(sub_device))

    if CONF_STATE_ON in config:
        state_on = yield state_hex_expression(config[CONF_STATE_ON])
        cg.add(var.set_state_on(state_on))

    if CONF_STATE_OFF in config:
        state_off = yield state_hex_expression(config[CONF_STATE_OFF])
        cg.add(var.set_state_off(state_off))

    if CONF_COMMAND_ON in config:
        data = config[CONF_COMMAND_ON]
        if cg.is_template(data):
            command_on = yield cg.templatable(data, [], cmd_t)
            cg.add(var.set_command_on(command_on))
        else:
            command_on = yield command_hex_expression(config[CONF_COMMAND_ON])
            cg.add(var.set_command_on(command_on))

    if CONF_COMMAND_OFF in config:
        data = config[CONF_COMMAND_OFF]
        if cg.is_template(data):
            command_off = yield cg.templatable(data, [], cmd_t)
            cg.add(var.set_command_off(command_off))
        else:
            command_off = yield command_hex_expression(config[CONF_COMMAND_OFF])
            cg.add(var.set_command_off(command_off))

    if CONF_COMMAND_STATE in config:
        command_state = yield command_hex_expression(config[CONF_COMMAND_STATE])
        cg.add(var.set_command_state(command_state))
    
    if CONF_STATE_RESPONSE in config:
        state_response = yield state_hex_expression(config[CONF_STATE_RESPONSE])
        cg.add(var.set_state_response(state_response))


@coroutine
def state_hex_expression(conf):
    if conf is None:
        return
    data = conf[CONF_DATA]
    and_operator = conf[CONF_AND_OPERATOR]
    inverted = conf[CONF_INVERTED]
    offset = conf[CONF_OFFSET]
    yield offset, and_operator, inverted, data


@coroutine
def command_hex_expression(conf):
    if conf is None:
        return
    data = conf[CONF_DATA]
    if CONF_ACK in conf:
        ack = conf[CONF_ACK]
        yield data, ack
    else:
        yield data


@automation.register_action('uartex.write', UARTExWriteAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(UARTExComponent),
    cv.Required(CONF_DATA): cv.templatable(validate_hex_data),
    cv.Optional(CONF_ACK, default=[]): validate_hex_data
}, key=CONF_DATA))
def uartex_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]

    if cg.is_template(data):
        templ = yield cg.templatable(data, args, cmd_t)
        cg.add(var.set_data_template(templ))
    else:
        cmd = yield command_hex_expression(config)
        cg.add(var.set_data_static(cmd))
    yield var
