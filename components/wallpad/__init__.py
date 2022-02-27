import logging
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.const import CONF_ID, CONF_BAUD_RATE, CONF_OFFSET, CONF_DATA, \
    CONF_UPDATE_INTERVAL, CONF_DEVICE, CONF_INVERTED, CONF_NUMBER, CONF_RX_PIN, CONF_TX_PIN
from esphome.core import CORE, coroutine
from esphome.util import SimpleRegistry
from .const import CONF_DATA_BITS, CONF_PARITY, CONF_STOP_BITS, \
    CONF_RX_PREFIX, CONF_RX_SUFFIX, CONF_TX_PREFIX, CONF_TX_SUFFIX, \
    CONF_RX_CHECKSUM, CONF_RX_CHECKSUM2, CONF_RX_CHECKSUM_LAMBDA, \
    CONF_TX_CHECKSUM, CONF_TX_CHECKSUM2, CONF_TX_CHECKSUM_LAMBDA, \
    CONF_ACK, CONF_WALLPAD_ID, CONF_MODEL, \
    CONF_SUB_DEVICE, \
    CONF_STATE_ON, CONF_STATE_OFF, CONF_COMMAND_ON, CONF_COMMAND_OFF, \
    CONF_COMMAND_STATE, CONF_RX_WAIT, CONF_TX_WAIT, CONF_TX_RETRY_CNT, \
    CONF_STATE_RESPONSE, CONF_LENGTH, CONF_PRECISION, CONF_AND_OPERATOR, \
    CONF_CTRL_PIN, CONF_TX_INTERVAL

_LOGGER = logging.getLogger(__name__)

wallpad_ns = cg.esphome_ns.namespace('wallpad')
WallPadComponent = wallpad_ns.class_('WallPadComponent', cg.Component)
WallPadWriteAction = wallpad_ns.class_('WallPadWriteAction', automation.Action)
cmd_hex_t = wallpad_ns.class_('cmd_hex_t')
num_t_const = wallpad_ns.class_('num_t').operator('const')
uint8_const = cg.uint8.operator('const')
uint8_ptr_const = uint8_const.operator('ptr')

MULTI_CONF = False

# Validate HEX: uint8_t[]
Model = wallpad_ns.enum("Model")
MODELS = {
    "CUSTOM": Model.MODEL_CUSTOM,
    "KOCOM": Model.MODEL_KOCOM,
    "SDS": Model.MODEL_SDS,
}


def validate_hex_data(value):
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a list of bytes")


# State HEX (hex_t): int offset, uint8_t[] data
STATE_HEX_SCHEMA = cv.Schema({
    cv.Required(CONF_DATA): validate_hex_data,
    cv.Optional(CONF_OFFSET, default=0): cv.int_range(min=0, max=128),
    cv.Optional(CONF_AND_OPERATOR, default=False): cv.boolean,
    cv.Optional(CONF_INVERTED, default=False): cv.boolean
})


def shorthand_state_hex(value):
    value = validate_hex_data(value)
    return STATE_HEX_SCHEMA({CONF_DATA: value})


def state_hex_schema(value):
    if isinstance(value, dict):
        return STATE_HEX_SCHEMA(value)
    return shorthand_state_hex(value)


# Command HEX: uint8_t[] data, uint8_t[] ack
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

def validate_tx_pin(value):
    value = pins.internal_gpio_output_pin_schema(value)

    #  - esp8266: UART0 (TX: GPIO1, RX: GPIO3)
    #  - esp32: UART2 (TX: GPIO17, RX: GPIO16)
    if CORE.is_esp8266 and value[CONF_NUMBER] != 1:
        raise cv.Invalid("RX pin have to use GPIO1 on ESP8266.")
    return value

def validate_rx_pin(value):
    value = pins.internal_gpio_input_pin_schema(value)
    if CORE.is_esp8266 and value[CONF_NUMBER] != 3:
        raise cv.Invalid("TX pin have to use GPIO3 on ESP8266.")
    return value

# WallPad Schema
CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(WallPadComponent),
    cv.Required(CONF_BAUD_RATE): cv.int_range(min=1, max=115200),
    cv.Optional(CONF_TX_PIN, default=1 if CORE.is_esp8266 else 17): validate_tx_pin,
    cv.Optional(CONF_RX_PIN, default=3 if CORE.is_esp8266 else 16): validate_rx_pin,
    cv.Optional(CONF_DATA_BITS, default=8): cv.int_range(min=1, max=32),
    # 0:No parity, 2:Even, 3:Odd
    cv.Optional(CONF_PARITY, default=0): cv.int_range(min=0, max=3),
    cv.Optional(CONF_STOP_BITS, default=1): cv.int_range(min=0, max=1),
    cv.Optional(CONF_MODEL, default="custom"): cv.enum(MODELS, upper=True),
    cv.Optional(CONF_RX_WAIT, default=10): cv.int_range(min=1, max=2000),
    cv.Optional(CONF_TX_INTERVAL): cv.int_range(min=1, max=2000),
    cv.Optional(CONF_TX_WAIT): cv.int_range(min=1, max=2000),
    cv.Optional(CONF_TX_RETRY_CNT): cv.int_range(min=1, max=10),
    cv.Optional(CONF_CTRL_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RX_PREFIX): validate_hex_data,
    cv.Optional(CONF_RX_SUFFIX): validate_hex_data,
    cv.Optional(CONF_TX_PREFIX): validate_hex_data,
    cv.Optional(CONF_TX_SUFFIX): validate_hex_data,
    cv.Optional(CONF_RX_CHECKSUM): cv.templatable(cv.boolean),
    cv.Optional(CONF_RX_CHECKSUM_LAMBDA): cv.returning_lambda,
    cv.Optional(CONF_RX_CHECKSUM2): cv.templatable(cv.boolean),
    cv.Optional(CONF_TX_CHECKSUM): cv.templatable(cv.boolean),
    cv.Optional(CONF_TX_CHECKSUM_LAMBDA): cv.returning_lambda,
    cv.Optional(CONF_TX_CHECKSUM2): cv.templatable(cv.boolean),
    #cv.Optional(CONF_PACKET_MONITOR): cv.ensure_list(state_hex_schema),
    cv.Optional(CONF_STATE_RESPONSE): state_hex_schema,
}).extend(cv.COMPONENT_SCHEMA),
cv.has_at_least_one_key(CONF_TX_PIN, CONF_RX_PIN),
)

async def to_code(config):
    cg.add_global(wallpad_ns.using)
    var = cg.new_Pvariable(config[CONF_ID],
                           config[CONF_BAUD_RATE],
                           config[CONF_DATA_BITS],
                           config[CONF_PARITY],
                           config[CONF_STOP_BITS],
                           config[CONF_RX_WAIT])
    await cg.register_component(var, config)

    if CONF_TX_INTERVAL in config:
        cg.add(var.set_tx_interval(config[CONF_TX_INTERVAL]))
    if CONF_TX_WAIT in config:
        cg.add(var.set_tx_wait(config[CONF_TX_WAIT]))
    if CONF_TX_RETRY_CNT in config:
        cg.add(var.set_tx_retry_cnt(config[CONF_TX_RETRY_CNT]))

    if CONF_TX_PIN in config:
        tx_pin = await cg.gpio_pin_expression(config[CONF_TX_PIN])
        cg.add(var.set_tx_pin(tx_pin))

    if CONF_RX_PIN in config:
        rx_pin = await cg.gpio_pin_expression(config[CONF_RX_PIN])
        cg.add(var.set_rx_pin(rx_pin))
        
    if CONF_CTRL_PIN in config:
        ctrl_pin = await cg.gpio_pin_expression(config[CONF_CTRL_PIN])
        cg.add(var.set_ctrl_pin(ctrl_pin))

    if CONF_STATE_RESPONSE in config:
        state_response = await state_hex_expression(config[CONF_STATE_RESPONSE])
        cg.add(var.set_state_response(state_response))

    # if CONF_PACKET_MONITOR in config:
    #     sm = cg.new_Pvariable(config[CONF_PACKET_MONITOR_ID])
    #     await sm
    #     for conf in config[CONF_PACKET_MONITOR]:
    #         data = conf[CONF_DATA]
    #         and_operator = conf[CONF_AND_OPERATOR]
    #         inverted = conf[CONF_INVERTED]
    #         offset = conf[CONF_OFFSET]
    #         cg.add(sm.add_filter([offset, and_operator, inverted, data]))
    #     cg.add(var.register_listener(sm))
    
    if CONF_MODEL in config:
        cg.add(var.set_model(config[CONF_MODEL]))

    if CONF_RX_PREFIX in config:
        cg.add(var.set_rx_prefix(config[CONF_RX_PREFIX]))
    if CONF_RX_SUFFIX in config:
        cg.add(var.set_rx_suffix(config[CONF_RX_SUFFIX]))

    if CONF_TX_PREFIX in config:
        cg.add(var.set_tx_prefix(config[CONF_TX_PREFIX]))
    if CONF_TX_SUFFIX in config:
        cg.add(var.set_tx_suffix(config[CONF_TX_SUFFIX]))

    if CONF_RX_CHECKSUM_LAMBDA in config:
        _LOGGER.warning(CONF_RX_CHECKSUM_LAMBDA +
                        " is deprecated and will be removed in a future version.")
        template_ = await cg.process_lambda(config[CONF_RX_CHECKSUM_LAMBDA],
                                            [(uint8_ptr_const, 'data'),
                                             (num_t_const, 'len')],
                                            return_type=cg.uint8)
        cg.add(var.set_rx_checksum_lambda(template_))
    if CONF_RX_CHECKSUM in config:
        data = config[CONF_RX_CHECKSUM]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (num_t_const, 'len')],
                                                return_type=cg.uint8)
            cg.add(var.set_rx_checksum_lambda(template_))
        else:
            cg.add(var.set_rx_checksum(data))

    if CONF_RX_CHECKSUM2 in config:
        data = config[CONF_RX_CHECKSUM2]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'), (num_t_const,
                                                                             'len'), (uint8_const, 'checksum1')],
                                                return_type=cg.uint8)
            cg.add(var.set_rx_checksum2_lambda(template_))
        else:
            cg.add(var.set_rx_checksum2(data))

    if CONF_TX_CHECKSUM_LAMBDA in config:
        _LOGGER.warning(CONF_TX_CHECKSUM_LAMBDA +
                        " is deprecated and will be removed in a future version.")
        template_ = await cg.process_lambda(config[CONF_TX_CHECKSUM_LAMBDA],
                                            [(uint8_ptr_const, 'data'),
                                             (num_t_const, 'len')],
                                            return_type=cg.uint8)
        cg.add(var.set_tx_checksum_lambda(template_))
    if CONF_TX_CHECKSUM in config:
        data = config[CONF_TX_CHECKSUM]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'),
                                                 (num_t_const, 'len')],
                                                return_type=cg.uint8)
            cg.add(var.set_tx_checksum_lambda(template_))
        else:
            cg.add(var.set_tx_checksum(data))

    if CONF_TX_CHECKSUM2 in config:
        data = config[CONF_TX_CHECKSUM2]
        if cg.is_template(data):
            template_ = await cg.process_lambda(data,
                                                [(uint8_ptr_const, 'data'), (num_t_const,
                                                                             'len'), (uint8_const, 'checksum1')],
                                                return_type=cg.uint8)
            cg.add(var.set_tx_checksum2_lambda(template_))
        else:
            cg.add(var.set_tx_checksum2(data))



# A schema to use for all WallPad devices, all WallPad integrations must extend this!
WallPad_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_WALLPAD_ID): cv.use_id(WallPadComponent),
    cv.Optional(CONF_DEVICE): state_hex_schema,
    cv.Optional(CONF_SUB_DEVICE): state_hex_schema,
    cv.Optional(CONF_STATE_ON): state_hex_schema,
    cv.Optional(CONF_STATE_OFF): state_hex_schema,
    cv.Required(CONF_COMMAND_ON): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_STATE): command_hex_schema,
}).extend(cv.polling_component_schema('60s'))

STATE_NUM_SCHEMA = cv.Schema({
    cv.Required(CONF_OFFSET): cv.int_range(min=0, max=128),
    cv.Optional(CONF_LENGTH, default=1): cv.int_range(min=1, max=4),
    cv.Optional(CONF_PRECISION, default=0): cv.int_range(min=0, max=5)
})


HEX_SCHEMA_REGISTRY = SimpleRegistry()


@coroutine
def register_wallpad_device(var, config):
    paren = yield cg.get_variable(config[CONF_WALLPAD_ID])
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
            command_on = yield cg.templatable(data, [], cmd_hex_t)
            cg.add(var.set_command_on(command_on))
        else:
            command_on = yield command_hex_expression(config[CONF_COMMAND_ON])
            cg.add(var.set_command_on(command_on))

    if CONF_COMMAND_OFF in config:
        data = config[CONF_COMMAND_OFF]
        if cg.is_template(data):
            command_off = yield cg.templatable(data, [], cmd_hex_t)
            cg.add(var.set_command_off(command_off))
        else:
            command_off = yield command_hex_expression(config[CONF_COMMAND_OFF])
            cg.add(var.set_command_off(command_off))

    if CONF_COMMAND_STATE in config:
        command_state = yield command_hex_expression(config[CONF_COMMAND_STATE])
        cg.add(var.set_command_state(command_state))


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


@automation.register_action('wallpad.write', WallPadWriteAction, cv.maybe_simple_value({
    cv.GenerateID(): cv.use_id(WallPadComponent),
    cv.Required(CONF_DATA): cv.templatable(validate_hex_data),
    cv.Optional(CONF_ACK, default=[]): validate_hex_data
}, key=CONF_DATA))
def wallpad_write_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_ID])
    data = config[CONF_DATA]

    if cg.is_template(data):
        templ = yield cg.templatable(data, args, cmd_hex_t)
        cg.add(var.set_data_template(templ))
    else:
        cmd = yield command_hex_expression(config)
        cg.add(var.set_data_static(cmd))
    yield var
