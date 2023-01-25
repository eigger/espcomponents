import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_MAC_ADDRESS,
)

#DEPENDENCIES = [""]

divoom_ns = cg.esphome_ns.namespace("divoom")
divoom = divoom_ns.class_(
    "DivoomDisplay", cg.PollingComponent, display.DisplayBuffer
)

Divoom16x16 = divoom_ns.class_("Divoom16x16", divoom)
Divoom11x11 = divoom_ns.class_("Divoom11x11", divoom)

DivoomModel = divoom_ns.enum("DivoomModel")

MODELS = {
    "16X16": DivoomModel.DIVOOM16,
    "11X11": DivoomModel.DIVOOM11,
}

Divoom_MODEL = cv.enum(MODELS, upper=True, space="_")

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(divoom),
            cv.Required(CONF_MODEL): Divoom_MODEL,
            cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        }
    )
    .extend(cv.polling_component_schema("1s")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    if config[CONF_MODEL] == "16X16":
        lcd_type = Divoom16x16
    if config[CONF_MODEL] == "11X11":
        lcd_type = Divoom11x11
    rhs = lcd_type.new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    await cg.register_component(var, config)
    await display.register_display(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    cg.add_library("BluetoothSerial", None)
