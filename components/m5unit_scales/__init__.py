import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["i2c"]

m5unit_scales_ns = cg.esphome_ns.namespace("m5unit_scales")
M5UnitScalesComponent = m5unit_scales_ns.class_(
    "M5UnitScalesComponent", cg.PollingComponent, i2c.I2CDevice
)

M5UnitScalesModel = m5unit_scales_ns.enum("M5UnitScalesModel")
MODELS = {
    "MINI": M5UnitScalesModel.M5UNIT_SCALES_MODEL_MINI,
    "STANDARD": M5UnitScalesModel.M5UNIT_SCALES_MODEL_STANDARD,
}

CONF_M5UNIT_SCALES_ID = "m5unit_scales_id"
CONF_MODEL = "model"

CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(M5UnitScalesComponent),
        cv.Required(CONF_MODEL): cv.enum(MODELS, upper=True),
    })
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x26))
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
