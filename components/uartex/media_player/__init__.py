import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import media_player, uartex
from esphome import core
from esphome.const import CONF_ID
from .. import uartex_ns, cmd_t, uint8_ptr_const, uint16_const, \
    command_hex_schema, command_hex_expression, state_schema, state_hex_expression
from ..const import CONF_STATE_NONE, CONF_STATE_IDLE, CONF_STATE_PLAYING, CONF_STATE_PAUSED, CONF_STATE_ANNOUNCING, \
    CONF_COMMAND_STOP, CONF_COMMAND_PLAY, CONF_COMMAND_PAUSE, CONF_COMMAND_MUTE, CONF_COMMAND_UNMUTE, CONF_COMMAND_TOGGLE, \
    CONF_COMMAND_VOLUME_UP, CONF_COMMAND_VOLUME_DOWN, CONF_COMMAND_ENQUEUE, CONF_COMMAND_REPEAT_ONE, CONF_COMMAND_REPEAT_OFF, \
    CONF_COMMAND_CLEAR_PLAYLIST, CONF_COMMAND_ON, CONF_COMMAND_OFF, CONF_STATE_ON, CONF_STATE_OFF, CONF_STATE_VOLUME

DEPENDENCIES = ['uartex']
UARTExMediaPlayer = uartex_ns.class_('UARTExMediaPlayer', media_player.MediaPlayer, cg.Component)

CONFIG_SCHEMA = cv.All(media_player.MEDIA_PLAYER_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTExMediaPlayer),
    cv.Optional(CONF_STATE_NONE): state_schema,
    cv.Optional(CONF_STATE_IDLE): state_schema,
    cv.Optional(CONF_STATE_PLAYING): state_schema,
    cv.Optional(CONF_STATE_PAUSED): state_schema,
    cv.Optional(CONF_STATE_ANNOUNCING): state_schema,
    cv.Required(CONF_STATE_VOLUME): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_STOP): command_hex_schema,
    cv.Optional(CONF_COMMAND_PLAY): command_hex_schema,
    cv.Optional(CONF_COMMAND_PAUSE): command_hex_schema,
    cv.Optional(CONF_COMMAND_MUTE): command_hex_schema,
    cv.Optional(CONF_COMMAND_UNMUTE): command_hex_schema,
    cv.Optional(CONF_COMMAND_TOGGLE): command_hex_schema,
    cv.Optional(CONF_COMMAND_VOLUME_UP): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_VOLUME_DOWN): cv.returning_lambda,
    cv.Optional(CONF_COMMAND_ENQUEUE): command_hex_schema,
    cv.Optional(CONF_COMMAND_REPEAT_ONE): command_hex_schema,
    cv.Optional(CONF_COMMAND_REPEAT_OFF): command_hex_schema,
    cv.Optional(CONF_COMMAND_CLEAR_PLAYLIST): command_hex_schema,
}).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Media Player do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Media Player do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Media Player do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Media Player do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_NONE in config:
        args = state_hex_expression(config[CONF_STATE_NONE])
        cg.add(var.set_state(CONF_STATE_NONE, args))

    if CONF_STATE_IDLE in config:
        args = state_hex_expression(config[CONF_STATE_IDLE])
        cg.add(var.set_state(CONF_STATE_IDLE, args))

    if CONF_STATE_PLAYING in config:
        args = state_hex_expression(config[CONF_STATE_PLAYING])
        cg.add(var.set_state(CONF_STATE_PLAYING, args))

    if CONF_STATE_PAUSED in config:
        args = state_hex_expression(config[CONF_STATE_PAUSED])
        cg.add(var.set_state(CONF_STATE_PAUSED, args))

    if CONF_STATE_ANNOUNCING in config:
        args = state_hex_expression(config[CONF_STATE_ANNOUNCING])
        cg.add(var.set_state(CONF_STATE_ANNOUNCING, args))

    if CONF_STATE_VOLUME in config:
        templ = await cg.templatable(config[CONF_STATE_VOLUME], [(uint8_ptr_const, 'data'), (uint16_const, 'len')], cg.float_)
        cg.add(var.set_state(CONF_STATE_VOLUME, templ))

    if CONF_COMMAND_STOP in config:
        args = command_hex_expression(config[CONF_COMMAND_STOP])
        cg.add(var.set_command(CONF_COMMAND_STOP, args))

    if CONF_COMMAND_PLAY in config:
        args = command_hex_expression(config[CONF_COMMAND_PLAY])
        cg.add(var.set_command(CONF_COMMAND_PLAY, args))

    if CONF_COMMAND_PAUSE in config:
        args = command_hex_expression(config[CONF_COMMAND_PAUSE])
        cg.add(var.set_command(CONF_COMMAND_PAUSE, args))

    if CONF_COMMAND_MUTE in config:
        args = command_hex_expression(config[CONF_COMMAND_MUTE])
        cg.add(var.set_command(CONF_COMMAND_MUTE, args))
        
    if CONF_COMMAND_UNMUTE in config:
        args = command_hex_expression(config[CONF_COMMAND_UNMUTE])
        cg.add(var.set_command(CONF_COMMAND_UNMUTE, args))
        
    if CONF_COMMAND_TOGGLE in config:
        args = command_hex_expression(config[CONF_COMMAND_TOGGLE])
        cg.add(var.set_command(CONF_COMMAND_TOGGLE, args))
        
    if CONF_COMMAND_VOLUME_UP in config:
        templ = await cg.templatable(config[CONF_COMMAND_VOLUME_UP], [(cg.float_.operator('const'), 'x')], cmd_t)
        cg.add(var.set_command(CONF_COMMAND_VOLUME_UP, templ))
        
    if CONF_COMMAND_VOLUME_DOWN in config:
        templ = await cg.templatable(config[CONF_COMMAND_VOLUME_DOWN], [(cg.float_.operator('const'), 'x')], cmd_t)
        cg.add(var.set_command(CONF_COMMAND_VOLUME_DOWN, templ))
        
    if CONF_COMMAND_ENQUEUE in config:
        args = command_hex_expression(config[CONF_COMMAND_ENQUEUE])
        cg.add(var.set_command(CONF_COMMAND_ENQUEUE, args))
        
    if CONF_COMMAND_REPEAT_ONE in config:
        args = command_hex_expression(config[CONF_COMMAND_REPEAT_ONE])
        cg.add(var.set_command(CONF_COMMAND_REPEAT_ONE, args))
        
    if CONF_COMMAND_REPEAT_OFF in config:
        args = command_hex_expression(config[CONF_COMMAND_REPEAT_OFF])
        cg.add(var.set_command(CONF_COMMAND_REPEAT_OFF, args))
        
    if CONF_COMMAND_CLEAR_PLAYLIST in config:
        args = command_hex_expression(config[CONF_COMMAND_CLEAR_PLAYLIST])
        cg.add(var.set_command(CONF_COMMAND_CLEAR_PLAYLIST, args))