import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, text_sensor, binary_sensor
from esphome.components.text_sensor import register_text_sensor
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_MAC_ADDRESS,
    CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, CONF_DEVICE_CLASS, ICON_NEW_BOX, CONF_STATUS
)

AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@eigger"]
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
            cv.Optional(CONF_VERSION, default={CONF_NAME: "Version"}): text_sensor.TEXT_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(text_sensor.TextSensor),
                cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
                cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
            }),
            cv.Optional(CONF_STATUS, default={CONF_NAME: "BT Status"}):  binary_sensor.BINARY_SENSOR_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(binary_sensor.BinarySensor),
                #cv.Optional(CONF_ICON, default=ICON_NEW_BOX): cv.icon,
                cv.Optional(CONF_ENTITY_CATEGORY, default="diagnostic"): cv.entity_category,
                cv.Optional(CONF_DEVICE_CLASS, default="connectivity"): binary_sensor.validate_device_class,
            }),
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
    if CONF_VERSION in config:
        sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
        await register_text_sensor(sens, config[CONF_VERSION])
        cg.add(var.set_version(sens))
    if CONF_STATUS in config:
        sens = cg.new_Pvariable(config[CONF_STATUS][CONF_ID])
        await binary_sensor.register_binary_sensor(sens, config[CONF_STATUS])
        cg.add(var.set_bt_status(sens))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    cg.add_library("BluetoothSerial", None)
