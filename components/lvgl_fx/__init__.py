import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["lvgl"]

lvgl_fx_ns = cg.esphome_ns.namespace("lvgl_fx")
LvglFx = lvgl_fx_ns.class_("LvglFx", cg.Component)
StartAction = lvgl_fx_ns.class_("StartAction", automation.Action)

CONF_MOTION = "motion"
CONF_FAST = "fast"
CONF_NORMAL = "normal"
CONF_SLOW = "slow"
CONF_INTENSITY = "intensity"
CONF_SUPPRESS_WHEN = "suppress_when"
CONF_MAX_TARGETS = "max_targets"
CONF_SLOTS_IN_PSRAM = "slots_in_psram"

# Durations are stored as uint16 milliseconds; the top values are reserved for the duration tokens (lvgl_fx.h).
MAX_DURATION_MS = 60000

_DURATION = cv.All(
    cv.positive_time_period_milliseconds,
    cv.Range(max=cv.TimePeriod(milliseconds=MAX_DURATION_MS)),
)

MOTION_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FAST, default="120ms"): _DURATION,
        cv.Optional(CONF_NORMAL, default="200ms"): _DURATION,
        cv.Optional(CONF_SLOW, default="320ms"): _DURATION,
        cv.Optional(CONF_INTENSITY, default=1.0): cv.float_range(min=0.0, max=4.0),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(LvglFx),
        cv.Optional(CONF_MOTION, default={}): MOTION_SCHEMA,
        cv.Optional(CONF_SUPPRESS_WHEN): cv.returning_lambda,
        cv.Optional(CONF_MAX_TARGETS, default=32): cv.int_range(min=1, max=255),
        cv.Optional(CONF_SLOTS_IN_PSRAM, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)


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
    if CONF_SUPPRESS_WHEN in config:
        # A plain function pointer: the lambda must not capture anything.
        lam = await cg.process_lambda(
            config[CONF_SUPPRESS_WHEN], [], return_type=cg.bool_, capture=""
        )
        cg.add(var.set_suppress_when(lam))

    # `fx::set_visible(...)` in user lambdas (DESIGN.md 3.2).
    cg.add_global(cg.RawStatement("namespace fx = esphome::lvgl_fx;"))


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
