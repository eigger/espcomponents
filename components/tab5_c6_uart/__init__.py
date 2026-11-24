import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import esp32
from esphome.const import CONF_ID

DEPENDENCIES = ["esp32", "i2c"]
ns = cg.esphome_ns.namespace("tab5_c6_uart")
Tab5C6Uart = ns.class_("Tab5C6Uart", cg.Component)
CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(Tab5C6Uart)}).extend(
    cv.COMPONENT_SCHEMA
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    esp32.include_builtin_idf_component("esp_driver_uart")
    esp32.include_builtin_idf_component("esp_driver_usb_serial_jtag")
