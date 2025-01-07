import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uartex
from esphome.const import CONF_ID
from .. import uartex_ns

DEPENDENCIES = ['uartex']
UARTExSwitch = uartex_ns.class_('UARTExSwitch', switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExSwitch),
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    await uartex.register_uartex_device(var, config)
