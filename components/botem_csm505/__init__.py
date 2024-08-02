import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uartex
from esphome.const import CONF_ID
from esphome.components.uartex.const import CONF_RX_HEADER, CONF_RX_FOOTER
from esphome.components.uartex import validate_hex_data

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["uartex"]
CONFIG_SCHEMA = cv.All(uartex.CONFIG_SCHEMA.extend({
    cv.Optional(CONF_RX_HEADER, default=[0x5E, 0x5B]): validate_hex_data,
    cv.Optional(CONF_RX_FOOTER, default=[0x5D, 0x0D]): validate_hex_data,
}))