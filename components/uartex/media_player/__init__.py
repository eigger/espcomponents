import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import media_player, uartex
from esphome.const import CONF_ID
from .. import uartex_ns, UARTExDevice, \
    state_schema, state_num_schema, state_hex_expression, state_num_expression, \
    command_hex_schema, command_expression, command_float_expression
from ..const import CONF_STATE_NONE, CONF_STATE_IDLE, CONF_STATE_PLAYING, CONF_STATE_PAUSED, CONF_STATE_ANNOUNCING, \
    CONF_COMMAND_STOP, CONF_COMMAND_PLAY, CONF_COMMAND_PAUSE, CONF_COMMAND_MUTE, CONF_COMMAND_UNMUTE, CONF_COMMAND_TOGGLE, \
    CONF_COMMAND_VOLUME, CONF_COMMAND_VOLUME_UP, CONF_COMMAND_VOLUME_DOWN, CONF_COMMAND_ENQUEUE, CONF_COMMAND_REPEAT_ONE, CONF_COMMAND_REPEAT_OFF, \
    CONF_COMMAND_CLEAR_PLAYLIST, CONF_COMMAND_ON, CONF_COMMAND_OFF, CONF_STATE_ON, CONF_STATE_OFF, CONF_STATE_VOLUME

DEPENDENCIES = ['uartex']
UARTExMediaPlayer = uartex_ns.class_('UARTExMediaPlayer', media_player.MediaPlayer, UARTExDevice)

CONFIG_SCHEMA = cv.All(media_player.media_player_schema(UARTExMediaPlayer).extend(uartex.UARTEX_DEVICE_SCHEMA).extend({
    cv.Optional(CONF_STATE_NONE): state_schema,
    cv.Optional(CONF_STATE_IDLE): state_schema,
    cv.Optional(CONF_STATE_PLAYING): state_schema,
    cv.Optional(CONF_STATE_PAUSED): state_schema,
    cv.Optional(CONF_STATE_ANNOUNCING): state_schema,
    cv.Optional(CONF_STATE_VOLUME): cv.templatable(state_num_schema),
    cv.Optional(CONF_COMMAND_STOP): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PLAY): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_PAUSE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_MUTE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_UNMUTE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_TOGGLE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_VOLUME): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_VOLUME_UP): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_VOLUME_DOWN): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ENQUEUE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_REPEAT_ONE): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_REPEAT_OFF): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_CLEAR_PLAYLIST): cv.templatable(command_hex_schema),
    cv.Optional(CONF_COMMAND_ON): cv.invalid("UARTEx Media Player do not support command_on!"),
    cv.Optional(CONF_COMMAND_OFF): cv.invalid("UARTEx Media Player do not support command_off!"),
    cv.Optional(CONF_STATE_ON): cv.invalid("UARTEx Media Player do not support state_on!"),
    cv.Optional(CONF_STATE_OFF): cv.invalid("UARTEx Media Player do not support state_off!")
}).extend(cv.COMPONENT_SCHEMA))

async def to_code(config):
    var = await media_player.new_media_player(config)
    await cg.register_component(var, config)
    await uartex.register_uartex_device(var, config)

    if CONF_STATE_NONE in config:
        state = state_hex_expression(config[CONF_STATE_NONE])
        cg.add(var.set_state(CONF_STATE_NONE, state))

    if CONF_STATE_IDLE in config:
        state = state_hex_expression(config[CONF_STATE_IDLE])
        cg.add(var.set_state(CONF_STATE_IDLE, state))

    if CONF_STATE_PLAYING in config:
        state = state_hex_expression(config[CONF_STATE_PLAYING])
        cg.add(var.set_state(CONF_STATE_PLAYING, state))

    if CONF_STATE_PAUSED in config:
        state = state_hex_expression(config[CONF_STATE_PAUSED])
        cg.add(var.set_state(CONF_STATE_PAUSED, state))

    if CONF_STATE_ANNOUNCING in config:
        state = state_hex_expression(config[CONF_STATE_ANNOUNCING])
        cg.add(var.set_state(CONF_STATE_ANNOUNCING, state))

    if CONF_STATE_VOLUME in config:
        state = await state_num_expression(config[CONF_STATE_VOLUME])
        cg.add(var.set_state(CONF_STATE_VOLUME, state))

    if CONF_COMMAND_STOP in config:
        command = await command_expression(config[CONF_COMMAND_STOP])
        cg.add(var.set_command(CONF_COMMAND_STOP, command))

    if CONF_COMMAND_PLAY in config:
        command = await command_expression(config[CONF_COMMAND_PLAY])
        cg.add(var.set_command(CONF_COMMAND_PLAY, command))

    if CONF_COMMAND_PAUSE in config:
        command = await command_expression(config[CONF_COMMAND_PAUSE])
        cg.add(var.set_command(CONF_COMMAND_PAUSE, command))

    if CONF_COMMAND_MUTE in config:
        command = await command_expression(config[CONF_COMMAND_MUTE])
        cg.add(var.set_command(CONF_COMMAND_MUTE, command))
        
    if CONF_COMMAND_UNMUTE in config:
        command = await command_expression(config[CONF_COMMAND_UNMUTE])
        cg.add(var.set_command(CONF_COMMAND_UNMUTE, command))
        
    if CONF_COMMAND_TOGGLE in config:
        command = await command_expression(config[CONF_COMMAND_TOGGLE])
        cg.add(var.set_command(CONF_COMMAND_TOGGLE, command))
        
    if CONF_COMMAND_VOLUME in config:
        command = await command_float_expression(config[CONF_COMMAND_VOLUME])
        cg.add(var.set_command(CONF_COMMAND_VOLUME, command))

    if CONF_COMMAND_VOLUME_UP in config:
        command = await command_float_expression(config[CONF_COMMAND_VOLUME_UP])
        cg.add(var.set_command(CONF_COMMAND_VOLUME_UP, command))
        
    if CONF_COMMAND_VOLUME_DOWN in config:
        command = await command_float_expression(config[CONF_COMMAND_VOLUME_DOWN])
        cg.add(var.set_command(CONF_COMMAND_VOLUME_DOWN, command))
        
    if CONF_COMMAND_ENQUEUE in config:
        command = await command_expression(config[CONF_COMMAND_ENQUEUE])
        cg.add(var.set_command(CONF_COMMAND_ENQUEUE, command))
        
    if CONF_COMMAND_REPEAT_ONE in config:
        command = await command_expression(config[CONF_COMMAND_REPEAT_ONE])
        cg.add(var.set_command(CONF_COMMAND_REPEAT_ONE, command))
        
    if CONF_COMMAND_REPEAT_OFF in config:
        command = await command_expression(config[CONF_COMMAND_REPEAT_OFF])
        cg.add(var.set_command(CONF_COMMAND_REPEAT_OFF, command))
        
    if CONF_COMMAND_CLEAR_PLAYLIST in config:
        command = await command_expression(config[CONF_COMMAND_CLEAR_PLAYLIST])
        cg.add(var.set_command(CONF_COMMAND_CLEAR_PLAYLIST, command))