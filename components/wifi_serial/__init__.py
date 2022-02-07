import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import pins
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority, CORE
from esphome.const import (
    CONF_PORT, CONF_ID, CONF_BAUD_RATE, CONF_RX_PIN, CONF_TX_PIN)

DEPENDENCIES = ['network']
AUTO_LOAD = ['async_tcp']

wifi_serial_ns = cg.esphome_ns.namespace('wifi_serial')
WifiSerial = wifi_serial_ns.class_('WifiSerial', cg.Component)

CONF_WIFI_SERIAL_ID = 'wifi_serial'
CONF_STOP_BITS = 'stop_bits'

def validate_rx_pin(value):
    value = pins.internal_gpio_input_pin_schema(value)
    if CORE.is_esp8266 and value >= 16:
        raise cv.Invalid("Pins GPIO16 and GPIO17 cannot be used as RX pins on ESP8266.")
    return value

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WifiSerial),
    cv.Optional(CONF_PORT, default=80): cv.port,
    cv.Required('usart'): cv.Schema({
        cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
        cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_RX_PIN): validate_rx_pin,
        cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
    }),
}).extend(cv.COMPONENT_SCHEMA)

@coroutine_with_priority(60.0)
def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    if 'usart' in config:
        cg.add(var.set_baud_rate(config['usart'][CONF_BAUD_RATE]))
        cg.add(var.set_tx_pin(config['usart'][CONF_TX_PIN]))
        cg.add(var.set_rx_pin(config['usart'][CONF_RX_PIN]))
        cg.add(var.set_stop_bits(config['usart'][CONF_STOP_BITS]))
