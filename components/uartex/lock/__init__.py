import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import lock, uartex
from esphome.const import CONF_ID
from .. import uartex_ns, command_hex_schema, \
    command_hex_expression, state_schema, state_hex_expression
from ..const import CONF_COMMAND_LOCK, CONF_COMMAND_OFF, CONF_COMMAND_UNLOCK, CONF_STATE_JAMMED, CONF_STATE_LOCKED, CONF_STATE_LOCKING, CONF_STATE_OFF, \
    CONF_COMMAND_ON, CONF_STATE_ON, CONF_STATE_UNLOCKED, CONF_STATE_UNLOCKING

DEPENDENCIES = ['uartex']
UARTExLock = uartex_ns.class_('UARTExLock', lock.Lock, cg.Component)

CONFIG_SCHEMA = cv.All(lock.LOCK_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExLock),
    cv.Optional(CONF_STATE_LOCKED): state_schema,
    cv.Optional(CONF_STATE_UNLOCKED): state_schema,
    cv.Optional(CONF_STATE_JAMMED): state_schema,
    cv.Optional(CONF_STATE_LOCKING): state_schema,
    cv.Optional(CONF_STATE_UNLOCKING): state_schema,
    cv.Optional(CONF_COMMAND_LOCK): command_hex_schema,
    cv.Optional(CONF_COMMAND_UNLOCK): command_hex_schema,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Lock do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Lock do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Lock do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Lock do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_STATE_LOCKED, CONF_STATE_UNLOCKED), cv.has_at_least_one_key(CONF_COMMAND_LOCK, CONF_COMMAND_UNLOCK))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await lock.register_lock(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_LOCKED in config:
        args = state_hex_expression(config[CONF_STATE_LOCKED])
        cg.add(var.set_state_locked(args))
    if CONF_STATE_UNLOCKED in config:
        args = state_hex_expression(config[CONF_STATE_UNLOCKED])
        cg.add(var.set_state_unlocked(args))
    if CONF_STATE_JAMMED in config:
        args = state_hex_expression(config[CONF_STATE_JAMMED])
        cg.add(var.set_state_jammed(args))
    if CONF_STATE_LOCKING in config:
        args = state_hex_expression(config[CONF_STATE_LOCKING])
        cg.add(var.set_state_locking(args))
    if CONF_STATE_UNLOCKING in config:
        args = state_hex_expression(config[CONF_STATE_UNLOCKING])
        cg.add(var.set_state_unlocking(args))
    if CONF_COMMAND_LOCK in config:
        args = command_hex_expression(config[CONF_COMMAND_LOCK])
        cg.add(var.set_command_lock(args))
    if CONF_COMMAND_UNLOCK in config:
        args = command_hex_expression(config[CONF_COMMAND_UNLOCK])
        cg.add(var.set_command_unlock(args))