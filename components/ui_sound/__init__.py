"""ui_sound: short UI cue sounds (clicks, success, warnings) through a speaker media player.  See README.md.

Each cue is a small heap object created at setup (file, priority, cooldown, duration); the component owns the policy
(priority interrupt, busy window, cooldown, min gap, mute / volume 0, enable, start).  Nothing here depends on LVGL.
"""

import esphome.codegen as cg
from esphome.components.speaker.media_player import SpeakerMediaPlayer
from esphome.components import audio
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_FILE, CONF_ID, CONF_MEDIA_PLAYER, CONF_PRIORITY

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["speaker"]

ui_sound_ns = cg.esphome_ns.namespace("ui_sound")
UiSound = ui_sound_ns.class_("UiSound", cg.Component)
Cue = ui_sound_ns.class_("Cue")
PlayAction = ui_sound_ns.class_("PlayAction", automation.Action)
StopAction = ui_sound_ns.class_("StopAction", automation.Action)
StartAction = ui_sound_ns.class_("StartAction", automation.Action)
SetEnabledAction = ui_sound_ns.class_("SetEnabledAction", automation.Action)
IsPlayingCondition = ui_sound_ns.class_("IsPlayingCondition", automation.Condition)

CONF_ANNOUNCEMENT = "announcement"
CONF_MIN_GAP = "min_gap"
CONF_START_GRACE = "start_grace"
CONF_MAX_BUSY = "max_busy"
CONF_REQUIRE_START = "require_start"
CONF_SKIP_WHEN_MUTED = "skip_when_muted"
CONF_CUES = "cues"
CONF_COOLDOWN = "cooldown"
CONF_DURATION = "duration"
CONF_CUE = "cue"
CONF_FORCE = "force"
CONF_VALUE = "value"

# Times are stored as uint32 milliseconds; one hour is far beyond any UI sound.
_TIME = cv.All(cv.positive_time_period_milliseconds, cv.Range(max=cv.TimePeriod(hours=1)))

CUE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(Cue),
        cv.Required(CONF_FILE): cv.use_id(audio.AudioFile),
        cv.Optional(CONF_PRIORITY, default=1): cv.int_range(min=0, max=255),
        cv.Optional(CONF_COOLDOWN, default="0ms"): _TIME,
        cv.Optional(CONF_DURATION, default="0ms"): _TIME,
    }
)


def _validate(config):
    ids = [cue[CONF_ID].id for cue in config[CONF_CUES]]
    if len(set(ids)) != len(ids):
        raise cv.Invalid("cue ids must be unique")
    if config[CONF_MAX_BUSY].total_milliseconds < 1:
        raise cv.Invalid(f"'{CONF_MAX_BUSY}' must be at least 1ms")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UiSound),
            cv.Required(CONF_MEDIA_PLAYER): cv.use_id(SpeakerMediaPlayer),
            cv.Optional(CONF_ANNOUNCEMENT, default=True): cv.boolean,
            cv.Optional(CONF_MIN_GAP, default="0ms"): _TIME,
            cv.Optional(CONF_START_GRACE, default="300ms"): _TIME,
            cv.Optional(CONF_MAX_BUSY, default="10s"): _TIME,
            cv.Optional(CONF_REQUIRE_START, default=False): cv.boolean,
            cv.Optional(CONF_SKIP_WHEN_MUTED, default=False): cv.boolean,
            cv.Required(CONF_CUES): cv.All(cv.ensure_list(CUE_SCHEMA), cv.Length(min=1)),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_media_player(await cg.get_variable(config[CONF_MEDIA_PLAYER])))
    cg.add(var.set_announcement(config[CONF_ANNOUNCEMENT]))
    cg.add(var.set_min_gap(config[CONF_MIN_GAP].total_milliseconds))
    cg.add(var.set_start_grace(config[CONF_START_GRACE].total_milliseconds))
    cg.add(var.set_max_busy(config[CONF_MAX_BUSY].total_milliseconds))
    cg.add(var.set_require_start(config[CONF_REQUIRE_START]))
    cg.add(var.set_skip_when_muted(config[CONF_SKIP_WHEN_MUTED]))
    for cue in config[CONF_CUES]:
        file = await cg.get_variable(cue[CONF_FILE])
        cg.new_Pvariable(
            cue[CONF_ID],
            file,
            cue[CONF_PRIORITY],
            cue[CONF_COOLDOWN].total_milliseconds,
            cue[CONF_DURATION].total_milliseconds,
        )


PLAY_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(UiSound),
        cv.Required(CONF_CUE): cv.use_id(Cue),
        cv.Optional(CONF_FORCE, default=False): cv.templatable(cv.boolean),
    },
    key=CONF_CUE,
)


@automation.register_action("ui_sound.play", PlayAction, PLAY_SCHEMA, synchronous=True)
async def play_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parent(paren))
    cg.add(var.set_cue(await cg.get_variable(config[CONF_CUE])))
    cg.add(var.set_force(await cg.templatable(config[CONF_FORCE], args, bool)))
    return var


_PARENT_ONLY = automation.maybe_simple_id({cv.GenerateID(): cv.use_id(UiSound)})


async def _parent_only_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parent(paren))
    return var


automation.register_action("ui_sound.stop", StopAction, _PARENT_ONLY, synchronous=True)(_parent_only_to_code)
automation.register_action("ui_sound.start", StartAction, _PARENT_ONLY, synchronous=True)(_parent_only_to_code)
automation.register_condition("ui_sound.is_playing", IsPlayingCondition, _PARENT_ONLY)(_parent_only_to_code)


@automation.register_action(
    "ui_sound.set_enabled",
    SetEnabledAction,
    cv.maybe_simple_value(
        {cv.GenerateID(): cv.use_id(UiSound), cv.Required(CONF_VALUE): cv.templatable(cv.boolean)},
        key=CONF_VALUE,
    ),
    synchronous=True,
)
async def set_enabled_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_parent(paren))
    cg.add(var.set_value(await cg.templatable(config[CONF_VALUE], args, bool)))
    return var
