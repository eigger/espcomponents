import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uartex
from esphome.const import CONF_ID
from .. import uartex_ns, UARTExDevice

DEPENDENCIES = ['uartex']
UARTExSwitch = uartex_ns.class_('UARTExSwitch', switch.Switch, UARTExDevice)

CONFIG_SCHEMA = switch.switch_schema(UARTExSwitch).extend(uartex.UARTEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var =  await switch.new_switch(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

# Validate cross-field requirements after the platform schema is applied.
CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, uartex.validate_entity_preserve_state)
