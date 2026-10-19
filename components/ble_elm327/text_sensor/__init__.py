import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from .. import BleElm327Component
from ..const import CONF_BLE_ELM327_ID, CONF_LAST_ERROR

DEPENDENCIES = ["ble_elm327"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BLE_ELM327_ID): cv.use_id(BleElm327Component),
        cv.Optional(CONF_LAST_ERROR): text_sensor.text_sensor_schema(
            icon="mdi:alert-outline",
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_BLE_ELM327_ID])
    if CONF_LAST_ERROR in config:
        sens = await text_sensor.new_text_sensor(config[CONF_LAST_ERROR])
        cg.add(parent.set_last_error_text_sensor(sens))
