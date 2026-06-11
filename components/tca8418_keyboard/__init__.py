import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import i2c, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_TRIGGER_ID
)

CONF_PRESS_KEY = "press_key"
CONF_RELEASE_KEY = "release_key"
CONF_IRQ_PIN = "irq_pin"
CONF_MODEL = "model"
CONF_ON_KEY_PRESS = "on_key_press"
CONF_ON_KEY_RELEASE = "on_key_release"
AUTO_LOAD = ["text_sensor"]
CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

tca8418_keyboard_ns = cg.esphome_ns.namespace("tca8418_keyboard")

Model = tca8418_keyboard_ns.enum("Model")
MODELS = {
    "cardputer_adv": Model.MODEL_CARDPUTER_ADV,
}

TCA8418Keyboard = tca8418_keyboard_ns.class_("TCA8418Keyboard", cg.Component, i2c.I2CDevice)
KeyPressTrigger = tca8418_keyboard_ns.class_(
    "KeyPressTrigger", automation.Trigger.template(cg.std_string)
)
KeyReleaseTrigger = tca8418_keyboard_ns.class_(
    "KeyReleaseTrigger", automation.Trigger.template(cg.std_string)
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TCA8418Keyboard),
            cv.Optional(CONF_MODEL, default="cardputer_adv"): cv.enum(MODELS, lower=True),
            cv.Optional(CONF_PRESS_KEY): text_sensor.text_sensor_schema(
                icon="mdi:keyboard"
            ),
            cv.Optional(CONF_RELEASE_KEY): text_sensor.text_sensor_schema(
                icon="mdi:keyboard"
            ),
            cv.Optional(CONF_IRQ_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_ON_KEY_PRESS): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(KeyPressTrigger),
                }
            ),
            cv.Optional(CONF_ON_KEY_RELEASE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(KeyReleaseTrigger),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x34))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    if CONF_PRESS_KEY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_PRESS_KEY])
        cg.add(var.set_press_key(sens))
    if CONF_RELEASE_KEY in config:
        sens = await text_sensor.new_text_sensor(config[CONF_RELEASE_KEY])
        cg.add(var.set_release_key(sens))
    if CONF_IRQ_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_IRQ_PIN])
        cg.add(var.set_irq_pin(pin))
    for conf in config.get(CONF_ON_KEY_PRESS, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
    for conf in config.get(CONF_ON_KEY_RELEASE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "x")], conf)
