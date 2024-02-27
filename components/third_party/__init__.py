import esphome.codegen as cg

CODEOWNERS = ["@eigger"]

third_party_ns = cg.esphome_ns.namespace('third_party')

async def to_code(config):
    cg.add_global(third_party_ns.using)