import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@eigger"]

third_party_ns = cg.esphome_ns.namespace('third_party')
ThirdPartyComponent = third_party_ns.class_('ThirdPartyComponent', cg.Component)

CONFIG_SCHEMA = cv.All(cv.Schema({
    cv.GenerateID(): cv.declare_id(ThirdPartyComponent),

}).extend(cv.COMPONENT_SCHEMA))


async def to_code(config):
    cg.add_global(third_party_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)