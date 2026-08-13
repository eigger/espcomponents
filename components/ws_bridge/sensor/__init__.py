import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, ws_bridge
from esphome.const import CONF_ACCURACY_DECIMALS

from .. import ws_bridge_ns
from ..const import CONF_SENSOR_ID

DEPENDENCIES = ["ws_bridge"]
WsBridgeSensor = ws_bridge_ns.class_("WsBridgeSensor", sensor.Sensor, cg.Component, ws_bridge.WsBridgeDevice)

CONFIG_SCHEMA = (
    sensor.sensor_schema(WsBridgeSensor)
    .extend(ws_bridge.WS_BRIDGE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            # Wrap an existing ESPHome sensor and expose it to Home Assistant.
            # device_class/unit_of_measurement/state_class/accuracy_decimals
            # are inherited from it unless also set here.
            cv.Optional(CONF_SENSOR_ID): cv.use_id(sensor.Sensor),
        }
    )
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await ws_bridge.register_ws_bridge_device(var, config)
    if CONF_SENSOR_ID in config:
        cg.add(var.set_source(await cg.get_variable(config[CONF_SENSOR_ID])))
    # 0 is both Sensor's default and a legitimate value, so presence in YAML
    # has to be a separate flag — see ws_declare_sensor().
    if CONF_ACCURACY_DECIMALS in config:
        cg.add(var.set_accuracy_overridden(True))
