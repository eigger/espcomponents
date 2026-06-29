import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from . import M5UnitScalesComponent, CONF_M5UNIT_SCALES_ID, m5unit_scales_ns

DEPENDENCIES = ["m5unit_scales"]

CONF_AVG_FILTER = "avg_filter"
CONF_EMA_FILTER = "ema_filter"
CONF_GAP = "gap"

M5UnitScalesAvgFilterNumber = m5unit_scales_ns.class_(
    "M5UnitScalesAvgFilterNumber", number.Number
)
M5UnitScalesEmaFilterNumber = m5unit_scales_ns.class_(
    "M5UnitScalesEmaFilterNumber", number.Number
)
M5UnitScalesGapNumber = m5unit_scales_ns.class_(
    "M5UnitScalesGapNumber", number.Number
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_M5UNIT_SCALES_ID): cv.use_id(M5UnitScalesComponent),
    cv.Optional(CONF_AVG_FILTER): number.number_schema(
        M5UnitScalesAvgFilterNumber,
    ),
    cv.Optional(CONF_EMA_FILTER): number.number_schema(
        M5UnitScalesEmaFilterNumber,
    ),
    cv.Optional(CONF_GAP): number.number_schema(
        M5UnitScalesGapNumber,
    ),
})

async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5UNIT_SCALES_ID])
    
    if CONF_AVG_FILTER in config:
        var = await number.new_number(
            config[CONF_AVG_FILTER],
            min_value=0,
            max_value=50,
            step=1,
        )
        cg.add(var.set_parent(parent))
        cg.add(parent.set_avg_filter_number(var))
        
    if CONF_EMA_FILTER in config:
        var = await number.new_number(
            config[CONF_EMA_FILTER],
            min_value=0,
            max_value=99,
            step=1,
        )
        cg.add(var.set_parent(parent))
        cg.add(parent.set_ema_filter_number(var))

    if CONF_GAP in config:
        var = await number.new_number(
            config[CONF_GAP],
            min_value=-1000000.0,
            max_value=1000000.0,
            step=0.00001,
        )
        cg.add(var.set_parent(parent))
        cg.add(parent.set_gap_number(var))
