import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, esp32_ble_tracker, display, text_sensor, binary_sensor
from esphome.components.text_sensor import register_text_sensor
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_MODEL,
    CONF_PAGES,
    CONF_VERSION, CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY, CONF_DEVICE_CLASS, 
    ICON_NEW_BOX, CONF_STATUS, CONF_CHARACTERISTIC_UUID, CONF_SERVICE_UUID
)

CONF_REQUIRE_RESPONSE = "require_response"

AUTO_LOAD = ["text_sensor", "binary_sensor"]
DEPENDENCIES = ["ble_client"]
CODEOWNERS = ["@eigger"]
gicisky_esl_ns = cg.esphome_ns.namespace("gicisky_esl")
gicisky_esl = gicisky_esl_ns.class_(
    "GiciskyESL", cg.PollingComponent, display.DisplayBuffer, ble_client.BLEClientNode
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(gicisky_esl),
            cv.Optional(CONF_SERVICE_UUID, default="49535343-FE7D-4AE5-8FA9-9FAFD205E455"): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_CHARACTERISTIC_UUID, default="49535343-8841-43F4-A8D4-ECBE34729BB3"): esp32_ble_tracker.bt_uuid,
            cv.Optional(CONF_REQUIRE_RESPONSE, default=True): cv.boolean,
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
    .extend(cv.polling_component_schema("1s"))
    .extend(ble_client.BLE_CLIENT_SCHEMA),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await display.register_display(var, config)
    await ble_client.register_ble_node(var, config)
    if len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(
            var.set_service_uuid16(esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID]))
        )
    elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid32_format):
        cg.add(
            var.set_service_uuid32(esp32_ble_tracker.as_hex(config[CONF_SERVICE_UUID]))
        )
    elif len(config[CONF_SERVICE_UUID]) == len(esp32_ble_tracker.bt_uuid128_format):
        uuid128 = esp32_ble_tracker.as_reversed_hex_array(config[CONF_SERVICE_UUID])
        cg.add(var.set_service_uuid128(uuid128))

    if len(config[CONF_CHARACTERISTIC_UUID]) == len(esp32_ble_tracker.bt_uuid16_format):
        cg.add(
            var.set_char_uuid16(
                esp32_ble_tracker.as_hex(config[CONF_CHARACTERISTIC_UUID])
            )
        )
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(
        esp32_ble_tracker.bt_uuid32_format
    ):
        cg.add(
            var.set_char_uuid32(
                esp32_ble_tracker.as_hex(config[CONF_CHARACTERISTIC_UUID])
            )
        )
    elif len(config[CONF_CHARACTERISTIC_UUID]) == len(
        esp32_ble_tracker.bt_uuid128_format
    ):
        uuid128 = esp32_ble_tracker.as_reversed_hex_array(
            config[CONF_CHARACTERISTIC_UUID]
        )
        cg.add(var.set_char_uuid128(uuid128))
    cg.add(var.set_require_response(config[CONF_REQUIRE_RESPONSE]))
    if CONF_VERSION in config:
        sens = cg.new_Pvariable(config[CONF_VERSION][CONF_ID])
        await register_text_sensor(sens, config[CONF_VERSION])
        cg.add(var.set_version(sens))
    if CONF_STATUS in config:
        sens = cg.new_Pvariable(config[CONF_STATUS][CONF_ID])
        await binary_sensor.register_binary_sensor(sens, config[CONF_STATUS])
        cg.add(var.set_bt_connected(sens))
    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
