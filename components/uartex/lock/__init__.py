import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import lock, uartex
from esphome import core
from esphome.const import CONF_ID
from .. import uartex_ns, UARTExDevice, \
    state_schema, state_hex_expression, \
    command_hex_schema, command_expression
from ..const import CONF_COMMAND_LOCK, CONF_COMMAND_OFF, CONF_COMMAND_UNLOCK, CONF_STATE_JAMMED, CONF_STATE_LOCKED, CONF_STATE_LOCKING, CONF_STATE_OFF, \
    CONF_COMMAND_ON, CONF_STATE_ON, CONF_STATE_UNLOCKED, CONF_STATE_UNLOCKING, CONF_LOCK_TIMEOUT, CONF_UNLOCK_TIMEOUT

DEPENDENCIES = ['uartex']
UARTExLock = uartex_ns.class_('UARTExLock', lock.Lock, UARTExDevice)

CONFIG_SCHEMA = cv.All(lock.lock_schema(UARTExLock).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE_LOCKED): state_schema,
    cv.Optional(CONF_STATE_UNLOCKED): state_schema,
    cv.Optional(CONF_STATE_JAMMED): state_schema,
    cv.Optional(CONF_STATE_LOCKING): state_schema,
    cv.Optional(CONF_STATE_UNLOCKING): state_schema,
    cv.Optional(CONF_COMMAND_LOCK): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_UNLOCK): cv.templatable(command_hex_schema),
    cv.Optional(CONF_LOCK_TIMEOUT, default="5s"): cv.All(
        cv.positive_time_period_milliseconds,
        cv.Range(max=core.TimePeriod(milliseconds=60 * 1000)),
    ),
    cv.Optional(CONF_UNLOCK_TIMEOUT, default="5s"): cv.All(
        cv.positive_time_period_milliseconds,
        cv.Range(max=core.TimePeriod(milliseconds=60 * 1000)),
    ),
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Lock do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Lock do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Lock do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Lock do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA), cv.has_at_least_one_key(CONF_STATE_LOCKED, CONF_STATE_UNLOCKED), cv.has_at_least_one_key(CONF_COMMAND_LOCK, CONF_COMMAND_UNLOCK))

async def to_code(config):
    var = await lock.new_lock(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_LOCKED in config:
        state = state_hex_expression(config[CONF_STATE_LOCKED])
        cg.add(var.set_state(CONF_STATE_LOCKED, state))

    if CONF_STATE_UNLOCKED in config:
        state = state_hex_expression(config[CONF_STATE_UNLOCKED])
        cg.add(var.set_state(CONF_STATE_UNLOCKED, state))

    if CONF_STATE_JAMMED in config:
        state = state_hex_expression(config[CONF_STATE_JAMMED])
        cg.add(var.set_state(CONF_STATE_JAMMED, state))

    if CONF_STATE_LOCKING in config:
        state = state_hex_expression(config[CONF_STATE_LOCKING])
        cg.add(var.set_state(CONF_STATE_LOCKING, state))

    if CONF_STATE_UNLOCKING in config:
        state = state_hex_expression(config[CONF_STATE_UNLOCKING])
        cg.add(var.set_state(CONF_STATE_UNLOCKING, state))

    if CONF_COMMAND_LOCK in config:
        command = await command_expression(config[CONF_COMMAND_LOCK])
        cg.add(var.set_command(CONF_COMMAND_LOCK, command))

    if CONF_COMMAND_UNLOCK in config:
        command = await command_expression(config[CONF_COMMAND_UNLOCK])
        cg.add(var.set_command(CONF_COMMAND_UNLOCK, command))

    if CONF_LOCK_TIMEOUT in config:
        cg.add(var.set_lock_timeout(config[CONF_LOCK_TIMEOUT]))
        
    if CONF_UNLOCK_TIMEOUT in config:
        cg.add(var.set_unlock_timeout(config[CONF_UNLOCK_TIMEOUT]))