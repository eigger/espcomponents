"""lvgl_fx: motion for ESPHome LVGL dashboards.  See README.md.

The definitions in the YAML (`transitions:`, `presets:`, `effects:`, `effect_defaults:`) become `static const` tables
in flash (7.6).  Only what the YAML says is emitted (designated initializers, in the declaration order of the structs
in lvgl_fx.h / effects.h); every field left out keeps its "unset" value, and the C++ side resolves it against the
configured defaults and its built-in table at run time.  Values the YAML does not give are never filled in here;
the only C++ layout facts mirrored in this file are the enum names and orders (Prop, Style, Curve ...) and the limits
of the fixed-width fields.
"""

import math
import re

import esphome.codegen as cg
from esphome.components.lvgl.lv_validation import (
    color as lv_color_validator,
    get_component_colors,
    lv_bool,
    lv_color,
    lv_float,
    lv_int,
    lv_text,
    opacity as lv_opacity,
)
from esphome.components.lvgl.defines import LValidator
from esphome.components.lvgl.lvcode import LambdaContext, lv_add
from esphome.components.lvgl.schemas import TEXT_SCHEMA
from esphome.components.lvgl.types import ObjUpdateAction, lv_obj_t
from esphome.components.lvgl.widgets import get_widgets
from esphome.components.lvgl.widgets.line import line_spec
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TRIGGER_ID
from esphome.core import CORE
from esphome import automation
from esphome.cpp_generator import AssignmentExpression, MockObj, RawExpression, RawStatement

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["lvgl"]

lvgl_fx_ns = cg.esphome_ns.namespace("lvgl_fx")
LvglFx = lvgl_fx_ns.class_("LvglFx", cg.Component)
StartAction = lvgl_fx_ns.class_("StartAction", automation.Action)
SetEnabledAction = lvgl_fx_ns.class_("SetEnabledAction", automation.Action)
SetReducedMotionAction = lvgl_fx_ns.class_("SetReducedMotionAction", automation.Action)
TransitionDef = lvgl_fx_ns.struct("TransitionDef")
EffectDef = lvgl_fx_ns.struct("EffectDef")
# `on_start:` / `on_end:` hooks pass the target widget; in lambdas it is `x`.
OnStartTrigger = automation.Trigger.template(lv_obj_t.operator("ptr"))
OnEndTrigger = automation.Trigger.template(lv_obj_t.operator("ptr"))

CONF_MOTION = "motion"
CONF_FAST = "fast"
CONF_NORMAL = "normal"
CONF_SLOW = "slow"
CONF_INTENSITY = "intensity"
CONF_SUPPRESS_WHEN = "suppress_when"
CONF_MAX_TARGETS = "max_targets"
CONF_SLOTS_IN_PSRAM = "slots_in_psram"
CONF_MAX_TEXT_RUNS = "max_text_runs"
CONF_MAX_TEXT_LENGTH = "max_text_length"
CONF_MAX_LINE_RUNS = "max_line_runs"
CONF_MAX_LINE_POINTS = "max_line_points"

CONF_TRANSITIONS = "transitions"
CONF_DEFAULTS = "defaults"
CONF_CUSTOM = "custom"
CONF_BINDINGS = "bindings"
CONF_PRESETS = "presets"
CONF_EFFECTS = "effects"
CONF_EFFECT_DEFAULTS = "effect_defaults"
CONF_TRACKS = "tracks"
CONF_THEN = "then"
CONF_DURATION = "duration"
CONF_DELAY = "delay"
CONF_CURVE = "curve"
CONF_STYLE = "style"
CONF_SHOW = "show"
CONF_HIDE = "hide"
CONF_FROM = "from"
CONF_DISTANCE = "distance"
CONF_FORMAT = "format"
CONF_CHAR_TIME = "char_time"
CONF_JITTER = "jitter"
CONF_NOISE = "noise"
CONF_NOISE_PERIOD = "noise_period"
CONF_SCALE = "scale"
CONF_X = "x"
CONF_Y = "y"
CONF_CYCLES = "cycles"
CONF_AXIS = "axis"
CONF_PROPERTY = "property"
CONF_COLOR = "color"
CONF_AMOUNT = "amount"
CONF_OPACITY = "opacity"
CONF_RETRIGGER = "retrigger"
CONF_WHEN_SUPPRESSED = "when_suppressed"
CONF_DEFER_TIMEOUT = "defer_timeout"
CONF_COOLDOWN = "cooldown"
CONF_COOLDOWN_SCOPE = "cooldown_scope"
CONF_ON_START = "on_start"
CONF_ON_END = "on_end"
CONF_POLICY = "policy"
CONF_REPEAT = "repeat"
CONF_REPEAT_DELAY = "repeat_delay"
CONF_EFFECT = "effect"
CONF_TRANSITION = "transition"
CONF_FORCE = "force"
CONF_VISIBLE = "visible"
CONF_VALUE = "value"
CONF_TEXT = "text"
CONF_ROTATION = "rotation"
CONF_POSITION = "position"
CONF_DRAW = "draw"

# Durations are stored as uint16 milliseconds; the top values are reserved for the duration tokens (effects.h).
MAX_DURATION_MS = 60000
# An effect's length is a uint16 in ms (effects.h: effect_length()).
MAX_EFFECT_MS = 65535
# Offsets are int16 sums (props.h) and styles clamp their travel to this (styles.cpp).
MAX_DISTANCE_PX = 1000
# `repeat: N` is a uint16 whose top value means "infinite" (effects.h REPEAT_INFINITE).
MAX_REPEAT = 65534
MAX_TRACKS = 255
MAX_CYCLES = 100
# printf output of a number is cut at 48 bytes (styles.cpp TEXT_BUF); a width beyond it is pointless.
MAX_FORMAT_WIDTH = 40
MAX_FORMAT_PRECISION = 6  # also what `count` can step through (4.2.1)
# decode uses at most this many bytes of its charset (styles.cpp MAX_NOISE)
MAX_NOISE_LEN = 32

DURATION_TOKENS = {CONF_FAST: "DUR_FAST", CONF_NORMAL: "DUR_NORMAL", CONF_SLOW: "DUR_SLOW"}
CURVES = ("linear", "ease_in", "ease_out", "ease_in_out")

# Order of enum class Prop (lvgl_fx.h): index = slot in TransitionDef::props.
TRANSITION_PROPS = (
    CONF_VISIBLE, CONF_OPACITY, CONF_VALUE, CONF_TEXT, CONF_COLOR, CONF_POSITION, CONF_ROTATION, CONF_DRAW
)
VISIBLE_STYLES = ("fade", "slide_up", "slide_down", "slide_left", "slide_right", "zoom", "none")
TEXT_STYLES = ("fade", "roll", "count", "typewriter", "decode", "none")
CHAR_STYLES = ("typewriter", "decode")  # the styles whose time can be given per character

BUILTIN_EFFECTS = {  # name -> (C++ constant, track kind)
    "pulse": ("PULSE", "pulse"),
    "shake": ("SHAKE", "shake"),
    "nudge_up": ("NUDGE_UP", "nudge"),
    "nudge_down": ("NUDGE_DOWN", "nudge"),
    "nudge_left": ("NUDGE_LEFT", "nudge"),
    "nudge_right": ("NUDGE_RIGHT", "nudge"),
    "flash": ("FLASH", "flash"),
    "glow": ("GLOW", "glow"),
    "blink": ("BLINK", "blink"),
}
# `distance` of a nudge preset is direction-relative: the signed (x, y) the track needs.
NUDGE_DIRECTION = {"nudge_up": (0, -1), "nudge_down": (0, 1), "nudge_left": (-1, 0), "nudge_right": (1, 0)}
TRACK_KINDS = ("pulse", "nudge", "shake", "flash", "glow", "blink", "image_pulse")
FLASH_PROPERTIES = {"bg_color": "BG_COLOR", "text_color": "TEXT_COLOR", "image_recolor": "IMAGE_RECOLOR"}

# ---- validators --------------------------------------------------------------------------------------------------

_DURATION_MS = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(max=cv.TimePeriod(milliseconds=MAX_DURATION_MS)),
)


def _duration(value):
    """A motion token name (`fast`, `normal`, `slow`) or a time.  Tokens stay tokens: they follow `fx::motion()`."""
    if isinstance(value, str) and value.lower() in DURATION_TOKENS:
        return value.lower()
    return _DURATION_MS(value)


def _time(value):
    return _DURATION_MS(value)


def _percent(low, high):
    """A whole percent, written `112%` (or as the fraction 1.12), within [low, high]."""

    def validator(value):
        if isinstance(value, str) and value.strip().endswith("%"):
            number = cv.float_(value.strip()[:-1])
        else:
            number = cv.float_(value) * 100
        if not math.isfinite(number):
            raise cv.Invalid(f"{value!r} is not a finite percentage")
        number = round(number, 6)
        if number != int(number):
            raise cv.Invalid(f"{value!r} is not a whole percent")
        if not low <= number <= high:
            raise cv.Invalid(f"{value!r} must be between {low}% and {high}% (write percentages like '112%')")
        return int(number)

    return validator


def _px(low, high):
    return cv.int_range(min=low, max=high)


def _rgb(value):
    """A color for a flash: a name, 0xRRGGBB or the id of a `color:`.  Resolved to a constant at code generation."""
    value = lv_color_validator(value)
    if isinstance(value, int) and not 0 <= value <= 0xFFFFFF:
        raise cv.Invalid(f"Color {value:#x} must be between 0x000000 and 0xFFFFFF")
    return value


# ASCII digits only: `\d` would also match other scripts' digits, which printf does not parse as a width.
_FORMAT_SPEC = re.compile(r"%([-+ #0]*)([0-9]*)(?:\.([0-9]*))?(.?)", re.DOTALL)


def _format(value):
    """A printf format with exactly one floating conversion (`%f`, `%.2f`, `%5.1f`) and optional `%%`.

    The format goes to snprintf with one double, so anything else (`%d`, `%s`, a second `%f`, `*`, a length
    modifier) is undefined behavior at run time; refuse it here.
    """
    value = cv.string(value)
    floats = 0
    i = 0
    while i < len(value):
        if value[i] != "%":
            i += 1
            continue
        if value.startswith("%%", i):
            i += 2
            continue
        match = _FORMAT_SPEC.match(value, i)
        flags, width, precision, conversion = match.groups()
        if conversion != "f":
            raise cv.Invalid(
                f"Format {value!r}: '{match.group(0)}' is not a floating conversion. Use exactly one '%f' "
                "(with optional flags, width and precision, e.g. '%.1f' or '%5.2f') and '%%' for a literal percent sign"
            )
        if width and int(width) > MAX_FORMAT_WIDTH:
            raise cv.Invalid(f"Format {value!r}: width {width} is too large (at most {MAX_FORMAT_WIDTH})")
        if precision and int(precision) > MAX_FORMAT_PRECISION:
            raise cv.Invalid(f"Format {value!r}: precision {precision} is too large (at most {MAX_FORMAT_PRECISION})")
        floats += 1
        i = match.end()
    if floats != 1:
        raise cv.Invalid(
            f"Format {value!r} must contain exactly one floating conversion such as '%.1f' (found {floats}); "
            "use '%%' for a literal percent sign"
        )
    return value


def _effect_ref(value):
    """A built-in effect name or the id of an `effects:` entry (checked to exist by ESPHome)."""
    if isinstance(value, str) and value in BUILTIN_EFFECTS:
        return value
    return cv.use_id(EffectDef)(value)


def _not_fx(id_):
    # `namespace fx = esphome::lvgl_fx;` is emitted at global scope; an id of the same name would redeclare it.
    if id_.id == "fx":
        raise cv.Invalid("'fx' is reserved (it is the C++ alias of the lvgl_fx namespace); choose another id")
    return id_


def _is_builtin(ref):
    return isinstance(ref, str)


# ---- transitions -------------------------------------------------------------------------------------------------

_TIMING = {
    cv.Optional(CONF_DURATION): _duration,
    cv.Optional(CONF_DELAY): _time,
    cv.Optional(CONF_CURVE): cv.one_of(*CURVES, lower=True),
}


def _style(styles):
    return cv.one_of(*styles, lower=True)


def _visible_entry(value):
    schema = cv.Schema(
        {
            **_TIMING,
            cv.Optional(CONF_STYLE): _style(VISIBLE_STYLES),
            cv.Optional(CONF_SHOW): _style(VISIBLE_STYLES),
            cv.Optional(CONF_HIDE): _style(VISIBLE_STYLES),
            cv.Optional(CONF_FROM): _percent(1, 200),  # zoom: the scale the widget starts at / goes to
            cv.Optional(CONF_DISTANCE): _px(0, MAX_DISTANCE_PX),
        }
    )
    value = schema(value)
    if CONF_STYLE in value and (CONF_SHOW in value or CONF_HIDE in value):
        raise cv.Invalid("Use either 'style' (showing and hiding) or 'show' and 'hide', not both")
    if CONF_SHOW in value and CONF_HIDE not in value:
        # An entry that names the show style but not the hide style would also set the hide style to it.
        raise cv.Invalid("'show' needs 'hide' next to it; use 'style' to set both")
    return value


_CHAR_TIME = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=5000)),
)
_NOISE_PERIOD = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(min=cv.TimePeriod(milliseconds=10), max=cv.TimePeriod(milliseconds=1000)),
)


def _noise(value):
    """The symbols a decode cycles through: printable ASCII only (a multi-byte character cannot replace one byte)."""
    value = cv.string_strict(value)
    if not value:
        raise cv.Invalid("'noise' must not be empty")
    if len(value) > MAX_NOISE_LEN:
        raise cv.Invalid(f"'noise' has {len(value)} characters; at most {MAX_NOISE_LEN}")
    if any(not 0x20 <= ord(char) <= 0x7E for char in value):
        raise cv.Invalid("'noise' must be printable ASCII (no control characters, no multi-byte characters)")
    return value


def _text_entry(value):
    value = cv.Schema(
        {
            **_TIMING,
            cv.Optional(CONF_STYLE): _style(TEXT_STYLES),
            cv.Optional(CONF_FROM): _percent(0, 100),  # opacity the new text starts at
            cv.Optional(CONF_DISTANCE): _px(0, MAX_DISTANCE_PX),
            cv.Optional(CONF_FORMAT): _format,
            cv.Optional(CONF_CHAR_TIME): _CHAR_TIME,  # typewriter / decode: ms per character
            cv.Optional(CONF_JITTER): cv.boolean,  # typewriter: uneven pace
            cv.Optional(CONF_NOISE): _noise,  # decode: symbols
            cv.Optional(CONF_NOISE_PERIOD): _NOISE_PERIOD,  # decode: ms between two noise changes
        }
    )(value)
    # An entry that sets no `style` inherits it from a lower level (the defaults), which cannot be known here: only
    # a style named in the same entry is checked.
    style = value.get(CONF_STYLE)
    if CONF_CHAR_TIME in value:
        if CONF_DURATION in value:
            raise cv.Invalid(
                "'char_time' and 'duration' both time the whole transition: use 'char_time' (ms per character, "
                "the duration follows the text length) or 'duration', not both"
            )
        if style is not None and style not in CHAR_STYLES:
            raise cv.Invalid(f"'char_time' only applies to the styles {' and '.join(CHAR_STYLES)}, not '{style}'")
    if CONF_JITTER in value and style is not None and style != "typewriter":
        raise cv.Invalid(f"'jitter' only applies to the style typewriter, not '{style}'")
    for key in (CONF_NOISE, CONF_NOISE_PERIOD):
        if key in value and style is not None and style != "decode":
            raise cv.Invalid(f"'{key}' only applies to the style decode, not '{style}'")
    return value


_PLAIN_ENTRY = cv.Schema(_TIMING)

_ENTRY_VALIDATORS = {
    CONF_VISIBLE: _visible_entry,
    CONF_TEXT: _text_entry,
    CONF_OPACITY: _PLAIN_ENTRY,
    CONF_VALUE: _PLAIN_ENTRY,
    CONF_COLOR: _PLAIN_ENTRY,
    CONF_POSITION: _PLAIN_ENTRY,
    CONF_ROTATION: _PLAIN_ENTRY,
    CONF_DRAW: _PLAIN_ENTRY,
}


def _entry(prop):
    validator = _ENTRY_VALIDATORS[prop]

    def entry(value):
        # `visible:` with nothing under it is a valid (empty) entry.
        return validator({} if value is None else value)

    return entry


_PROP_ENTRIES = {cv.Optional(prop): _entry(prop) for prop in TRANSITION_PROPS}

DEFAULTS_SCHEMA = cv.Schema(_PROP_ENTRIES)


_ON_START = automation.validate_automation(
    {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnStartTrigger)}
)
_ON_END = automation.validate_automation({cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(OnEndTrigger)})


def _custom_transition(value):
    value = cv.Schema(
        {
            cv.Required(CONF_ID): cv.All(cv.declare_id(TransitionDef), _not_fx),
            **_PROP_ENTRIES,
            cv.Optional(CONF_THEN): _effect_ref,
            cv.Optional(CONF_POLICY): cv.one_of("normal", "always", lower=True),
            cv.Optional(CONF_ON_START): _ON_START,
            cv.Optional(CONF_ON_END): _ON_END,
        }
    )(value)
    if not any(key in value for key in (*TRANSITION_PROPS, CONF_THEN)):
        raise cv.Invalid(
            f"Transition '{value[CONF_ID]}' sets nothing: give it at least one of {', '.join(TRANSITION_PROPS)} "
            f"or '{CONF_THEN}'"
        )
    return value


def _binding(value):
    value = cv.Schema({
        cv.Required(CONF_ID): cv.use_id(lv_obj_t),
        **{cv.Optional(prop): cv.use_id(TransitionDef) for prop in TRANSITION_PROPS},
    })(value)
    if not any(prop in value for prop in TRANSITION_PROPS):
        raise cv.Invalid("A transition binding needs at least one property")
    return _draw_needs_line(value)


def _unique_bindings(value):
    seen = set()
    for binding in value:
        for prop in TRANSITION_PROPS:
            key = (binding[CONF_ID].id, prop)
            if prop in binding and key in seen:
                raise cv.Invalid(f"Duplicate transition binding for {binding[CONF_ID]} / {prop}")
            if prop in binding:
                seen.add(key)
    return value


TRANSITIONS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DEFAULTS): DEFAULTS_SCHEMA,
        cv.Optional(CONF_CUSTOM): cv.ensure_list(_custom_transition),
        cv.Optional(CONF_BINDINGS): cv.All(cv.ensure_list(_binding), _unique_bindings),
    }
)

# ---- tracks and effects ------------------------------------------------------------------------------------------

_TRACK_COMMON = {
    cv.Optional(CONF_DURATION): _duration,
    cv.Optional(CONF_CURVE): cv.one_of(*CURVES, lower=True),
}
_TRACK_DELAY = {cv.Optional(CONF_DELAY): _time}

# Fields of every material; shared by the effect tracks and the `presets:` (which only drop the delay).
_PULSE_FIELDS = {cv.Optional(CONF_SCALE): _percent(10, 400)}
_NUDGE_FIELDS = {cv.Optional(CONF_X): _px(-MAX_DISTANCE_PX, MAX_DISTANCE_PX),
                 cv.Optional(CONF_Y): _px(-MAX_DISTANCE_PX, MAX_DISTANCE_PX)}
_SHAKE_FIELDS = {
    cv.Optional(CONF_DISTANCE): _px(0, MAX_DISTANCE_PX),
    cv.Optional(CONF_CYCLES): cv.int_range(min=1, max=MAX_CYCLES),
    cv.Optional(CONF_AXIS): cv.one_of("x", "y", lower=True),
}
_FLASH_FIELDS = {
    cv.Optional(CONF_PROPERTY): cv.one_of(*FLASH_PROPERTIES, lower=True),
    cv.Optional(CONF_COLOR): _rgb,
    cv.Optional(CONF_AMOUNT): _percent(0, 100),
    cv.Optional("profile"): cv.one_of("fade_out", "triangle", lower=True),
}
_OPACITY_FIELDS = {cv.Optional(CONF_OPACITY): _percent(0, 100)}

_KIND_FIELDS = {
    "pulse": _PULSE_FIELDS,
    "nudge": _NUDGE_FIELDS,
    "shake": _SHAKE_FIELDS,
    "flash": _FLASH_FIELDS,
    "image_pulse": {cv.Optional(CONF_AMOUNT): _percent(0, 100)},
    "glow": _OPACITY_FIELDS,
    "blink": _OPACITY_FIELDS,
}


def _cycles_message(schema):
    """`cycles: 0` is not "default": say so (0 is the unset value of the C++ table, so it must never come through)."""

    def validator(value):
        if isinstance(value, dict) and value.get(CONF_CYCLES) == 0:
            raise cv.Invalid("cycles: 0 is invalid (a shake needs at least 1 cycle); leave it out for the default")
        return schema(value)

    return validator


_TRACK_SCHEMAS = {
    kind: cv.Schema({**_TRACK_COMMON, **_TRACK_DELAY, **fields}) for kind, fields in _KIND_FIELDS.items()
}
_TRACK_SCHEMAS["shake"] = _cycles_message(_TRACK_SCHEMAS["shake"])


def _track(value):
    if not isinstance(value, dict) or len(value) != 1:
        raise cv.Invalid(
            f"A track is a single material with its fields, e.g. '- pulse: {{scale: 110%}}'; one of: "
            f"{', '.join(TRACK_KINDS)}"
        )
    ((kind, fields),) = value.items()
    kind = cv.one_of(*TRACK_KINDS, lower=True)(kind)
    try:
        track = dict(_TRACK_SCHEMAS[kind]({} if fields is None else fields))
    except cv.Invalid as err:
        raise cv.Invalid(err.msg, [kind, *err.path]) from err
    if kind == "nudge" and CONF_X not in track and CONF_Y not in track:
        raise cv.Invalid("A nudge needs 'x' and/or 'y' (px)")
    track["kind"] = kind
    return track


_OPTIONS = {
    cv.Optional(CONF_RETRIGGER): cv.one_of("restart", "ignore", lower=True),
    cv.Optional(CONF_WHEN_SUPPRESSED): cv.one_of("skip", "defer", lower=True),
    cv.Optional(CONF_DEFER_TIMEOUT): _time,
    cv.Optional(CONF_COOLDOWN): _time,
    cv.Optional(CONF_COOLDOWN_SCOPE): cv.one_of("effect", "widget", lower=True),
    cv.Optional(CONF_POLICY): cv.one_of("normal", "always", lower=True),
    cv.Optional(CONF_REPEAT_DELAY): _time,
}


def _repeat(value):
    if isinstance(value, str) and value.lower() == "infinite":
        return "infinite"
    return cv.int_range(min=1, max=MAX_REPEAT)(value)


def _explicit_length_ms(tracks, motion):
    """Lower bound of an effect's length from the numbers the YAML gives (a track without a duration counts 0)."""
    length = 0
    for track in tracks:
        duration = track.get(CONF_DURATION)
        if isinstance(duration, str):
            duration = motion[duration]
        else:
            duration = duration.total_milliseconds if duration is not None else 0
        delay = track[CONF_DELAY].total_milliseconds if CONF_DELAY in track else 0
        length = max(length, delay + duration)
    return length


def _effect(value):
    value = cv.Schema(
        {
            cv.Required(CONF_ID): cv.All(cv.declare_id(EffectDef), _not_fx),
            cv.Required(CONF_TRACKS): cv.All(cv.ensure_list(_track), cv.Length(min=1, max=MAX_TRACKS)),
            cv.Optional(CONF_REPEAT): _repeat,
            cv.Optional(CONF_THEN): _effect_ref,
            cv.Optional(CONF_ON_START): _ON_START,
            cv.Optional(CONF_ON_END): _ON_END,
            **_OPTIONS,
        }
    )(value)
    if value[CONF_ID].id in BUILTIN_EFFECTS:
        raise cv.Invalid(
            f"'{value[CONF_ID]}' is the name of a built-in effect; choose another id "
            "(change a built-in with 'presets:')",
            [CONF_ID],
        )
    if value.get(CONF_REPEAT) == "infinite" and CONF_THEN in value:
        raise cv.Invalid(
            "'then' cannot be used with 'repeat: infinite': a loop never ends, so nothing could follow it",
            [CONF_THEN],
        )
    if value.get(CONF_REPEAT) == "infinite" and CONF_ON_START in value:
        raise cv.Invalid(
            "'on_start' cannot be used with 'repeat: infinite': hooks are not supported on loops",
            [CONF_ON_START],
        )
    if value.get(CONF_REPEAT) == "infinite" and CONF_ON_END in value:
        raise cv.Invalid(
            "'on_end' cannot be used with 'repeat: infinite': a loop never ends, so nothing could follow it",
            [CONF_ON_END],
        )
    return value


EFFECT_DEFAULTS_SCHEMA = cv.Schema(
    {
        # An infinite default would turn every effect without its own `repeat` into a loop, which then could not
        # have a `then`.  Loops are per effect.
        cv.Optional(CONF_REPEAT): cv.int_range(min=1, max=MAX_REPEAT),
        **_OPTIONS,
    }
)

# `presets:` adjust the magnitudes of a built-in effect.  The built-in effect has a single track of one kind.
_PRESET_SCHEMAS = {
    name: cv.Schema({**_TRACK_COMMON, **_KIND_FIELDS[kind]}) for name, (_, kind) in BUILTIN_EFFECTS.items()
}
for _name in NUDGE_DIRECTION:
    # The direction is part of the effect: the distance is relative to it (`nudge_up: {distance: 6}` moves 6 px up).
    _PRESET_SCHEMAS[_name] = cv.Schema({**_TRACK_COMMON, cv.Optional(CONF_DISTANCE): _px(0, MAX_DISTANCE_PX)})
_PRESET_SCHEMAS["shake"] = _cycles_message(_PRESET_SCHEMAS["shake"])


def _preset(name):
    schema = _PRESET_SCHEMAS[name]

    def preset(value):
        value = schema({} if value is None else value)
        if not value:
            raise cv.Invalid(f"Preset '{name}' sets nothing")
        return value

    return preset


PRESETS_SCHEMA = cv.Schema({cv.Optional(name): _preset(name) for name in BUILTIN_EFFECTS})

# ---- component schema --------------------------------------------------------------------------------------------

MOTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FAST, default="120ms"): _DURATION_MS,
        cv.Optional(CONF_NORMAL, default="200ms"): _DURATION_MS,
        cv.Optional(CONF_SLOW, default="320ms"): _DURATION_MS,
        cv.Optional(CONF_INTENSITY, default=1.0): cv.float_range(min=0.0, max=4.0),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.All(cv.declare_id(LvglFx), _not_fx),
        cv.Optional(CONF_MOTION, default={}): MOTION_SCHEMA,
        cv.Optional(CONF_SUPPRESS_WHEN): cv.returning_lambda,
        cv.Optional(CONF_MAX_TARGETS, default=32): cv.int_range(min=1, max=255),
        cv.Optional(CONF_SLOTS_IN_PSRAM, default=False): cv.boolean,
        # typewriter / decode: static pool of text runs (styles.cpp), emitted as the defines the C++ side defaults
        cv.Optional(CONF_MAX_TEXT_RUNS, default=4): cv.int_range(min=1, max=32),
        # bytes of a run including the NUL; the character weights of a text must fit a uint16 (127 x 140 ... 399 x 140)
        cv.Optional(CONF_MAX_TEXT_LENGTH, default=128): cv.int_range(min=16, max=400),
        # line draw: static pool of partial point buffers (draw.cpp), emitted as the defines the C++ side defaults
        cv.Optional(CONF_MAX_LINE_RUNS, default=4): cv.int_range(min=1, max=32),
        cv.Optional(CONF_MAX_LINE_POINTS, default=16): cv.int_range(min=2, max=64),
        cv.Optional(CONF_TRANSITIONS): TRANSITIONS_SCHEMA,
        cv.Optional(CONF_PRESETS): PRESETS_SCHEMA,
        cv.Optional(CONF_EFFECTS): cv.ensure_list(_effect),
        cv.Optional(CONF_EFFECT_DEFAULTS): EFFECT_DEFAULTS_SCHEMA,
    }
).extend(cv.COMPONENT_SCHEMA)


def _final_validate(config):
    """Checks that need the whole definition: what a `then` points at, and the length of an effect."""
    effects = {eff[CONF_ID].id: eff for eff in config.get(CONF_EFFECTS, [])}
    motion = {name: config[CONF_MOTION][name].total_milliseconds for name in DURATION_TOKENS}

    def target(ref):
        return None if ref is None or _is_builtin(ref) else effects.get(ref.id)

    def is_loop(effect):
        return effect is not None and effect.get(CONF_REPEAT) == "infinite"

    for index, effect in enumerate(config.get(CONF_EFFECTS, [])):
        where = [CONF_EFFECTS, index]
        if _explicit_length_ms(effect[CONF_TRACKS], motion) > MAX_EFFECT_MS:
            raise cv.Invalid(
                f"Effect '{effect[CONF_ID]}' is longer than {MAX_EFFECT_MS} ms (the latest track delay + duration)",
                where + [CONF_TRACKS],
            )
        then = effect.get(CONF_THEN)
        if is_loop(target(then)):
            raise cv.Invalid(
                f"'then: {then}' is a loop (repeat: infinite), which never ends; it cannot follow an effect",
                where + [CONF_THEN],
            )
        # A chain of `then` that comes back to itself cannot be emitted (and would never rest).
        seen = [effect[CONF_ID].id]
        link = target(then)
        while link is not None:
            if link[CONF_ID].id in seen:
                chain = " -> ".join([*seen, link[CONF_ID].id])
                raise cv.Invalid(f"'then' chain loops back to itself: {chain}", where + [CONF_THEN])
            seen.append(link[CONF_ID].id)
            link = target(link.get(CONF_THEN))

    for index, transition in enumerate(config.get(CONF_TRANSITIONS, {}).get(CONF_CUSTOM, [])):
        then = transition.get(CONF_THEN)
        if is_loop(target(then)):
            raise cv.Invalid(
                f"'then: {then}' is a loop (repeat: infinite), which never ends; it cannot follow a transition",
                [CONF_TRANSITIONS, CONF_CUSTOM, index, CONF_THEN],
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate

# ---- code generation ---------------------------------------------------------------------------------------------


def _cpp(enum, name):
    return f"{lvgl_fx_ns}::{enum}::{str(name).upper()}"


def _ms(value):
    return str(value.total_milliseconds)


def _duration_cpp(value):
    return f"{lvgl_fx_ns}::{DURATION_TOKENS[value]}" if isinstance(value, str) else _ms(value)


def _init(fields):
    """`{.a = 1, .b = 2}` from (name, value) pairs; a value of None means "not set" and is left out."""
    return "{" + ", ".join(f".{name} = {value}" for name, value in fields if value is not None) + "}"


def _get(config, key, convert):
    return convert(config[key]) if key in config else None


def _ident(id_, suffix):
    """Name of a generated table object.

    `lvgl_fx_id_` keeps user ids apart from `lvgl_fx_defaults_*` / `lvgl_fx_preset_*`.  The user-visible id itself is a
    pointer to the `_def`."""
    return f"lvgl_fx_id_{id_.id}_{suffix}"


def _declare(id_, type_, definition):
    """Emit `static const <type> lvgl_fx_<id>_def = ...;` plus the id the user's lambdas see: a const pointer to it."""
    CORE.register_variable(id_, MockObj(id_, "->"))
    cg.add_global(RawStatement(f"static const {type_} {_ident(id_, 'def')} = {definition};"))
    cg.add_global(RawStatement(f"static const {type_} *const {id_} = &{_ident(id_, 'def')};"))


def _then_cpp(ref):
    if _is_builtin(ref):
        return f"&{lvgl_fx_ns}::{BUILTIN_EFFECTS[ref][0]}_DEF"
    return f"&{_ident(ref, 'def')}"


def _track_fields(kind, conf, x=None, y=None):
    """Fields of effects.h `Track` in declaration order, from a validated track (or preset) config."""
    fields = [
        ("kind", _cpp("TrackKind", kind)),
        ("curve", _get(conf, CONF_CURVE, lambda v: _cpp("Curve", v))),
        ("delay", _get(conf, CONF_DELAY, _ms)),
        ("duration", _get(conf, CONF_DURATION, _duration_cpp)),
        ("scale", _get(conf, CONF_SCALE, str)),
        ("x", x if x is not None else _get(conf, CONF_X, str)),
        ("y", y if y is not None else _get(conf, CONF_Y, str)),
        ("distance", None if kind == "nudge" else _get(conf, CONF_DISTANCE, str)),
        ("cycles", _get(conf, CONF_CYCLES, str)),
        ("axis", _get(conf, CONF_AXIS, lambda v: _cpp("Axis", v))),
        ("property", _get(conf, CONF_PROPERTY, lambda v: _cpp("FlashProp", FLASH_PROPERTIES[v]))),
        ("color", _get(conf, CONF_COLOR, _color_cpp)),
        ("amount", _get(conf, CONF_AMOUNT, str)),
        ("profile", _get(conf, "profile", lambda v: _cpp("FlashProfile", v))),
        ("opacity", _get(conf, CONF_OPACITY, str)),
    ]
    return fields


def _color_cpp(value):
    r, g, b = get_component_colors(value)
    return f"{(r << 16) | (g << 8) | b:#08x}"


def _option_fields(conf):
    return [
        ("retrigger", _get(conf, CONF_RETRIGGER, lambda v: _cpp("Retrigger", v))),
        ("when_suppressed", _get(conf, CONF_WHEN_SUPPRESSED, lambda v: _cpp("WhenSuppressed", v))),
        ("policy", _get(conf, CONF_POLICY, lambda v: _cpp("EffectPolicy", v))),
        ("repeat", _get(conf, CONF_REPEAT, _repeat_cpp)),
        ("repeat_delay", _get(conf, CONF_REPEAT_DELAY, _ms)),
        ("cooldown", _get(conf, CONF_COOLDOWN, _ms)),
        ("defer_timeout", _get(conf, CONF_DEFER_TIMEOUT, _ms)),
        ("cooldown_scope", _get(conf, CONF_COOLDOWN_SCOPE, lambda v: _cpp("CooldownScope", v))),
    ]


def _repeat_cpp(value):
    return f"{lvgl_fx_ns}::REPEAT_INFINITE" if value == "infinite" else str(value)


def _use_track(kind):
    cg.add_define(f"USE_LVGL_FX_{kind.upper()}")


async def _hook_to_code(id_, config, key):
    """Emit a captureless function that fires the `on_start:` / `on_end:` automation `key`; return its name (None
    without a hook).

    The Trigger is a global pointer, so the function needs no capture and fits the plain pointer in the const table.
    The hook is a list of automations like any ESPHome trigger; each gets its own Trigger, all fired in order."""
    if key not in config:
        return None
    triggers = []
    for conf in config[key]:
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await automation.build_automation(trigger, [(lv_obj_t.operator("ptr"), "x")], conf)
        triggers.append(f"{conf[CONF_TRIGGER_ID]}->trigger(target);")
    name = _ident(id_, key)
    cg.add_global(RawStatement(f"static void {name}(lv_obj_t *target) {{ {' '.join(triggers)} }}"))
    return name


async def _on_start_to_code(id_, config):
    return await _hook_to_code(id_, config, CONF_ON_START)


async def _on_end_to_code(id_, config):
    return await _hook_to_code(id_, config, CONF_ON_END)


async def _effect_to_code(effect):
    id_ = effect[CONF_ID]
    tracks = []
    for track in effect[CONF_TRACKS]:
        _use_track(track["kind"])
        tracks.append(_init(_track_fields(track["kind"], track)))
    cg.add_global(
        RawStatement(f"static const {lvgl_fx_ns}::Track {_ident(id_, 'tracks')}[] = {{" + ", ".join(tracks) + "};")
    )
    # Declaration order of EffectDef: the options, then `then`, then the fields added later.
    options = dict(_option_fields(effect))
    scope = options.pop("cooldown_scope")
    fields = [
        ("tracks", _ident(id_, "tracks")),
        ("track_count", str(len(tracks))),
        *options.items(),
        ("then", _get(effect, CONF_THEN, _then_cpp)),
        ("cooldown_scope", scope),
        ("on_start", await _on_start_to_code(id_, effect)),
        ("on_end", await _on_end_to_code(id_, effect)),
    ]
    _declare(id_, EffectDef, _init(fields))


def _ordered_effects(effects):
    """Effects with the ones a `then` points at first (a const table can only point at what is declared above)."""
    by_id = {eff[CONF_ID].id: eff for eff in effects}
    ordered, done = [], set()

    def visit(effect):
        if effect[CONF_ID].id in done:
            return
        done.add(effect[CONF_ID].id)
        then = effect.get(CONF_THEN)
        if then is not None and not _is_builtin(then):
            visit(by_id[then.id])
        ordered.append(effect)

    for effect in effects:
        visit(effect)
    return ordered


def _prop_fields(prop, conf):
    style = conf.get(CONF_STYLE) or conf.get(CONF_SHOW)
    return [
        ("duration", _get(conf, CONF_DURATION, _duration_cpp)),
        ("delay", _get(conf, CONF_DELAY, _ms)),
        ("curve", _get(conf, CONF_CURVE, lambda v: _cpp("Curve", v))),
        ("style", _cpp("Style", style) if style else None),
        # `style` alone means both: the C++ side takes the hide style from it when none is given.
        ("hide_style", _get(conf, CONF_HIDE, lambda v: _cpp("Style", v))),
        ("from", _get(conf, CONF_FROM, str)),
        ("distance", _get(conf, CONF_DISTANCE, str)),
        ("format", _get(conf, CONF_FORMAT, _cstring)),
        ("char_time", _get(conf, CONF_CHAR_TIME, _ms)),
        ("jitter", _get(conf, CONF_JITTER, lambda v: "1" if v else "0")),
        ("noise", _get(conf, CONF_NOISE, _cstring)),
        ("noise_period", _get(conf, CONF_NOISE_PERIOD, _ms)),
    ]


def _cstring(value):
    from esphome.helpers import cpp_string_escape

    return cpp_string_escape(value)


def _transition_table(prefix, config):
    """Emit the PropTransition entries of a definition (objects `<prefix>_<prop>`); return the `props` initializer."""
    entries = []
    for prop in TRANSITION_PROPS:
        if prop not in config:
            entries.append("nullptr")
            continue
        conf = config[prop]
        for key in (CONF_STYLE, CONF_SHOW, CONF_HIDE):
            if key in conf:
                CORE.data.setdefault("lvgl_fx_styles", set()).add(conf[key])
        object_name = f"{prefix}_{prop}"
        cg.add_global(RawStatement(f"static const {lvgl_fx_ns}::PropTransition {object_name} = "
                                   f"{_init(_prop_fields(prop, conf))};"))
        entries.append(f"&{object_name}")
    while entries and entries[-1] == "nullptr":
        entries.pop()
    return "{" + ", ".join(entries) + "}"


def _preset_to_code(name, conf):
    constant, kind = BUILTIN_EFFECTS[name]
    x = y = None
    if name in NUDGE_DIRECTION:
        sign_x, sign_y = NUDGE_DIRECTION[name]
        if CONF_DISTANCE in conf:
            x, y = (str(sign_x * conf[CONF_DISTANCE]), str(sign_y * conf[CONF_DISTANCE]))
            # Only the axis the direction moves along is adjusted: the other keeps the effect's own 0.
            x, y = (x if sign_x else None), (y if sign_y else None)
    _use_track(kind)
    track = f"lvgl_fx_preset_{name}"
    cg.add_global(
        RawStatement(f"static const {lvgl_fx_ns}::Track {track} = {_init(_track_fields(kind, conf, x, y))};")
    )
    cg.add(lvgl_fx_ns.set_preset(getattr(lvgl_fx_ns, constant), RawExpression(f"&{track}")))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    motion = config[CONF_MOTION]
    cg.add(
        var.set_motion(
            motion[CONF_FAST].total_milliseconds,
            motion[CONF_NORMAL].total_milliseconds,
            motion[CONF_SLOW].total_milliseconds,
            motion[CONF_INTENSITY],
        )
    )
    cg.add(var.set_max_targets(config[CONF_MAX_TARGETS]))
    cg.add(var.set_slots_in_psram(config[CONF_SLOTS_IN_PSRAM]))
    cg.add_define("LVGL_FX_TEXT_RUNS", config[CONF_MAX_TEXT_RUNS])
    cg.add_define("LVGL_FX_TEXT_RUN_BYTES", config[CONF_MAX_TEXT_LENGTH])
    cg.add_define("LVGL_FX_LINE_RUNS", config[CONF_MAX_LINE_RUNS])
    cg.add_define("LVGL_FX_LINE_POINTS", config[CONF_MAX_LINE_POINTS])
    if CONF_SUPPRESS_WHEN in config:
        # A plain function pointer: the lambda must not capture anything.
        lam = await cg.process_lambda(config[CONF_SUPPRESS_WHEN], [], return_type=cg.bool_, capture="")
        cg.add(var.set_suppress_when(lam))

    # `fx::set_visible(...)` in user lambdas.
    cg.add_global(RawStatement("namespace fx = esphome::lvgl_fx;"))

    for effect in _ordered_effects(config.get(CONF_EFFECTS, [])):
        await _effect_to_code(effect)

    transitions = config.get(CONF_TRANSITIONS, {})
    if CONF_DEFAULTS in transitions:
        table = _transition_table("lvgl_fx_defaults", transitions[CONF_DEFAULTS])
        cg.add_global(
            RawStatement(f"static const {lvgl_fx_ns}::TransitionDef lvgl_fx_defaults_def = {{.props = {table}}};")
        )
        cg.add(var.set_transition_defaults(RawExpression("&lvgl_fx_defaults_def")))
    for transition in transitions.get(CONF_CUSTOM, []):
        id_ = transition[CONF_ID]
        fields = [
            ("props", _transition_table(f"lvgl_fx_id_{id_.id}", transition)),
            ("then", _get(transition, CONF_THEN, _then_cpp)),
            ("policy", _get(transition, CONF_POLICY, lambda v: _cpp("EffectPolicy", v))),
            ("on_start", await _on_start_to_code(id_, transition)),
            ("on_end", await _on_end_to_code(id_, transition)),
        ]
        _declare(id_, TransitionDef, _init(fields))

    binding_index = 0
    for binding in transitions.get(CONF_BINDINGS, []):
        (widget,) = await get_widgets(binding)
        # A compound widget's `obj` stays null until the widget is created.  The pointer goes through a local so that a
        # statically allocated widget (whose address is never null) does not trigger -Waddress.
        if widget.type.is_compound():
            getter = f"[]() -> lv_obj_t * {{ auto *w = {widget.var}; return w != nullptr ? w->obj : nullptr; }}"
        else:
            getter = f"[]() -> lv_obj_t * {{ return {widget.obj}; }}"
        for prop in TRANSITION_PROPS:
            if prop not in binding:
                continue
            transition = await cg.get_variable(binding[prop])
            name = f"lvgl_fx_binding_{binding_index}"
            binding_index += 1
            cg.add(RawStatement(
                f"static {lvgl_fx_ns}::Binding {name} = "
                f"{{{getter}, {_cpp('Prop', prop)}, {transition}}};"
            ))
            cg.add(var.add_binding(RawExpression(f"&{name}")))

    for name, conf in config.get(CONF_PRESETS, {}).items():
        _preset_to_code(name, conf)

    for key, value in _option_fields(config.get(CONF_EFFECT_DEFAULTS, {})):
        if value is not None:
            cg.add(AssignmentExpression(None, "", f"{lvgl_fx_ns}::effect_defaults().{key}", RawExpression(value)))

    for style in CORE.data.get("lvgl_fx_styles", ()):
        cg.add_define(f"USE_LVGL_FX_STYLE_{style.upper()}")


# ---- actions -----------------------------------------------------------------------------------------------------


def _start_schema(value):
    # `- lvgl_fx.start:` and `- lvgl_fx.start: <id>` are both fine; the id is only validated, the API is global.
    if value is None:
        value = {}
    elif not isinstance(value, dict):
        value = {CONF_ID: value}
    return cv.Schema({cv.Optional(CONF_ID): cv.use_id(LvglFx)})(value)


@automation.register_action("lvgl_fx.start", StartAction, _start_schema, synchronous=True)
async def lvgl_fx_start_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


def _switch_schema():
    # `- lvgl_fx.set_enabled: false`, or `{value: !lambda ...}`; a lambda gets the trigger arguments.
    return cv.maybe_simple_value({cv.Required(CONF_VALUE): cv.templatable(cv.boolean)}, key=CONF_VALUE)


async def _switch_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_value(await cg.templatable(config[CONF_VALUE], args, bool)))
    return var


automation.register_action("lvgl_fx.set_enabled", SetEnabledAction, _switch_schema(), synchronous=True)(
    _switch_to_code
)
automation.register_action(
    "lvgl_fx.set_reduced_motion", SetReducedMotionAction, _switch_schema(), synchronous=True
)(_switch_to_code)


def _draw_percent(value):
    """`draw: 100%` / `33.3%`: the share of the line's path length, 0 - 100 (a lambda returns the number 0 - 100).

    Bare numbers are refused: `0.5` and `50` would be read differently by every other property."""
    if isinstance(value, str) and value.strip().endswith("%"):
        number = cv.float_(value.strip()[:-1])
        if not math.isfinite(number) or not 0 <= number <= 100:
            raise cv.Invalid(f"{value!r} must be between 0% and 100%")
        return number
    raise cv.Invalid(f"{value!r}: write the draw amount as a percentage ('100%', '33.3%') or a lambda returning 0 - 100")


lv_draw_percent = LValidator(_draw_percent, cg.float_)

# property -> (validator of the value (None: text, see TEXT_SCHEMA), C++ setter)
SET_PROPERTIES = {
    CONF_VISIBLE: (lv_bool, "set_visible"),
    CONF_OPACITY: (lv_opacity, "set_opacity"),
    CONF_VALUE: (lv_float, "set_value"),
    CONF_TEXT: (None, "set_text"),
    CONF_COLOR: (lv_color, "set_color"),
    CONF_X: (lv_int, "set_x"),
    CONF_Y: (lv_int, "set_y"),
    CONF_ROTATION: (lv_float, "set_rotation"),
    CONF_DRAW: (lv_draw_percent, "set_draw"),
}

_WIDGET = {cv.Required(CONF_ID): cv.use_id(lv_obj_t)}
_FORCE = {cv.Optional(CONF_FORCE): lv_bool}

def _draw_needs_line(value):
    """`draw` only exists for `line` widgets: the id is checked against the line type (a clear error, not a log line)."""
    if CONF_DRAW in value:
        value = {**value, CONF_ID: cv.use_id(line_spec.w_type)(value[CONF_ID].id)}
    return value


SET_SCHEMA = cv.All(
    cv.Schema(
        {
            **_WIDGET,
            **{
                cv.Optional(prop): validator
                for prop, (validator, _) in SET_PROPERTIES.items()
                if validator is not None
            },
            **TEXT_SCHEMA,
            cv.Optional(CONF_TRANSITION): cv.use_id(TransitionDef),
            cv.Optional("instant"): lv_bool,
            **_FORCE,
        }
    ),
    cv.has_exactly_one_key(*SET_PROPERTIES),
    _draw_needs_line,
)

PLAY_SCHEMA = cv.Schema({**_WIDGET, cv.Required(CONF_EFFECT): _effect_ref, **_FORCE})

STOP_SCHEMA = cv.maybe_simple_value(
    {**_WIDGET, cv.Optional(CONF_EFFECT): _effect_ref},
    key=CONF_ID,
)


async def _bool(value):
    # lv_bool maps a literal through `literal()`, which leaves a Python bool as `True`.
    if isinstance(value, bool):
        return RawExpression("true" if value else "false")
    return await lv_bool.process(value)


async def _opt(config):
    """Build options only when supplied; instant is available on set actions only."""
    if not any(key in config for key in (CONF_TRANSITION, CONF_FORCE, "instant")):
        return None
    transition = str(await cg.get_variable(config[CONF_TRANSITION])) if CONF_TRANSITION in config else "nullptr"
    force = await _bool(config[CONF_FORCE]) if CONF_FORCE in config else "false"
    instant = await _bool(config["instant"]) if "instant" in config else "false"
    return RawExpression(f"{lvgl_fx_ns}::Opt{{{transition}, {force}, false, {instant}}}")


async def _effect_expr(ref):
    if _is_builtin(ref):
        _use_track(BUILTIN_EFFECTS[ref][1])
        return RawExpression(f"{lvgl_fx_ns}::{BUILTIN_EFFECTS[ref][0]}")
    return await cg.get_variable(ref)


@automation.register_action("lvgl_fx.set", ObjUpdateAction, SET_SCHEMA, synchronous=True)
async def lvgl_fx_set_to_code(config, action_id, template_arg, args):
    (widget,) = await get_widgets(config)
    prop = next(key for key in SET_PROPERTIES if key in config)
    validator, setter = SET_PROPERTIES[prop]
    async with LambdaContext(args, where=action_id) as context:
        if prop == CONF_VISIBLE:
            value = await _bool(config[prop])
        else:
            value = await (validator if validator is not None else lv_text).process(config[prop])
        call_args = [widget.obj, value]
        if (opt := await _opt(config)) is not None:
            call_args.append(opt)
        lv_add(getattr(lvgl_fx_ns, setter)(*call_args))
    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())


@automation.register_action("lvgl_fx.play", ObjUpdateAction, PLAY_SCHEMA, synchronous=True)
async def lvgl_fx_play_to_code(config, action_id, template_arg, args):
    (widget,) = await get_widgets(config)
    async with LambdaContext(args, where=action_id) as context:
        call_args = [widget.obj, await _effect_expr(config[CONF_EFFECT])]
        if (opt := await _opt(config)) is not None:
            call_args.append(opt)
        lv_add(lvgl_fx_ns.play(*call_args))
    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())


@automation.register_action("lvgl_fx.stop", ObjUpdateAction, STOP_SCHEMA, synchronous=True)
async def lvgl_fx_stop_to_code(config, action_id, template_arg, args):
    (widget,) = await get_widgets(config)
    async with LambdaContext(args, where=action_id) as context:
        call_args = [widget.obj]
        if CONF_EFFECT in config:
            call_args.append(await _effect_expr(config[CONF_EFFECT]))
        lv_add(lvgl_fx_ns.stop(*call_args))
    return cg.new_Pvariable(action_id, template_arg, await context.get_lambda())
