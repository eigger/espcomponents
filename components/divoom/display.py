import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, text_sensor, binary_sensor, select, number
from esphome.components.text_sensor import register_text_sensor
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_MAC_ADDRESS,
    CONF_OPTION,
    CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, CONF_DEVICE_CLASS, ICON_NEW_BOX, CONF_STATUS
)
CONF_TYPE = "type"
CONF_BRIGHTNESS = "brightness"
AUTO_LOAD = ["text_sensor", "select", "binary_sensor", "number"]
CODEOWNERS = ["@eigger"]
divoom_ns = cg.esphome_ns.namespace("divoom")
divoom = divoom_ns.class_(
    "DivoomDisplay", cg.PollingComponent, display.DisplayBuffer
)

DivoomDitoo = divoom_ns.class_("DivoomDitoo", divoom)
Divoom11x11 = divoom_ns.class_("Divoom11x11", divoom)
SelectTime = divoom_ns.class_("SelectTime", divoom)
Brightness = divoom_ns.class_("Brightness", divoom)

DivoomModel = divoom_ns.enum("DivoomModel")

MODELS = {
    "ditoo": DivoomModel.DITOO,
    "11x11": DivoomModel.DIVOOM11,
}

Divoom_MODEL = cv.enum(MODELS, upper=False, space="_")

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(divoom),
            cv.Optional(CONF_MODEL, default="ditoo"): Divoom_MODEL,
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
            cv.Optional(CONF_TYPE, default={CONF_NAME: "Type"}):  select.SELECT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(SelectTime),
            }),
            cv.Optional(CONF_BRIGHTNESS, default={CONF_NAME: "Brightness"}):  number.NUMBER_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(Brightness),
            }),
        }
    )
    .extend(cv.polling_component_schema("1s")),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    if config[CONF_MODEL] == "ditoo":
        lcd_type = DivoomDitoo
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
        cg.add(var.set_bt_connected(sens))
    if CONF_TYPE in config:
        sens = cg.new_Pvariable(config[CONF_TYPE][CONF_ID])
        await select.register_select(sens, config[CONF_TYPE], options=[
            "Type1", 
            "Type2", 
            "Type3",
            "Type4", 
            "Type5",
            "Type6",
            "Type7",
            "Type8",
            "Type9",
            "Type10",
            "Type11",
            "Type12",
            "Type13",
            "Type14"])
        cg.add(var.set_select_time(sens))
    if CONF_BRIGHTNESS in config:
        sens = cg.new_Pvariable(config[CONF_BRIGHTNESS][CONF_ID])
        await number.register_number(sens, config[CONF_BRIGHTNESS],
            min_value = 0,
            max_value = 100,
            step = 0x01)
        cg.add(var.set_brightness(sens))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
    cg.add_library("BluetoothSerial", None)
