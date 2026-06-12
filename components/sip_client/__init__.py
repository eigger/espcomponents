import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import microphone, speaker
from esphome.const import (
    CONF_ID,
    CONF_PORT,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_MICROPHONE,
    CONF_SPEAKER,
    CONF_TRIGGER_ID,
)
from .const import (
    CONF_SERVER,
    CONF_DOMAIN,
    CONF_CALLER_ID,
    CONF_REGISTER_EXPIRATION,
    CONF_LOCAL_RTP_PORT,
    CONF_ON_REGISTERED,
    CONF_ON_INCOMING_CALL,
    CONF_ON_CALL_CONNECTED,
    CONF_ON_CALL_ENDED,
    CONF_ON_DTMF,
    CONF_NUMBER,
    CONF_DIGITS,
)

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket", "md5", "audio"]

sip_client_ns = cg.esphome_ns.namespace("sip_client")
SipClient = sip_client_ns.class_("SipClient", cg.Component)

RegisteredTrigger = sip_client_ns.class_("RegisteredTrigger", automation.Trigger.template())
IncomingCallTrigger = sip_client_ns.class_(
    "IncomingCallTrigger", automation.Trigger.template(cg.std_string)
)
CallConnectedTrigger = sip_client_ns.class_("CallConnectedTrigger", automation.Trigger.template())
CallEndedTrigger = sip_client_ns.class_("CallEndedTrigger", automation.Trigger.template())
DtmfTrigger = sip_client_ns.class_("DtmfTrigger", automation.Trigger.template(cg.std_string))

CallAction = sip_client_ns.class_("CallAction", automation.Action)
AnswerAction = sip_client_ns.class_("AnswerAction", automation.Action)
HangupAction = sip_client_ns.class_("HangupAction", automation.Action)
SendDtmfAction = sip_client_ns.class_("SendDtmfAction", automation.Action)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SipClient),
            cv.Required(CONF_MICROPHONE): cv.use_id(microphone.Microphone),
            cv.Required(CONF_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Required(CONF_SERVER): cv.string,
            cv.Optional(CONF_PORT, default=5060): cv.port,
            cv.Required(CONF_USERNAME): cv.string,
            cv.Required(CONF_PASSWORD): cv.string,
            cv.Optional(CONF_DOMAIN): cv.string,
            cv.Optional(CONF_CALLER_ID): cv.string,
            cv.Optional(
                CONF_REGISTER_EXPIRATION, default="300s"
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_LOCAL_RTP_PORT, default=7078): cv.port,
            cv.Optional(CONF_ON_REGISTERED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(RegisteredTrigger)}
            ),
            cv.Optional(CONF_ON_INCOMING_CALL): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(IncomingCallTrigger)}
            ),
            cv.Optional(CONF_ON_CALL_CONNECTED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CallConnectedTrigger)}
            ),
            cv.Optional(CONF_ON_CALL_ENDED): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CallEndedTrigger)}
            ),
            cv.Optional(CONF_ON_DTMF): automation.validate_automation(
                {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DtmfTrigger)}
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    mic = await cg.get_variable(config[CONF_MICROPHONE])
    cg.add(var.set_microphone(mic))
    spk = await cg.get_variable(config[CONF_SPEAKER])
    cg.add(var.set_speaker(spk))

    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    if CONF_DOMAIN in config:
        cg.add(var.set_domain(config[CONF_DOMAIN]))
    if CONF_CALLER_ID in config:
        cg.add(var.set_caller_id(config[CONF_CALLER_ID]))
    cg.add(var.set_register_expiration(config[CONF_REGISTER_EXPIRATION]))
    cg.add(var.set_local_rtp_port(config[CONF_LOCAL_RTP_PORT]))

    for conf in config.get(CONF_ON_REGISTERED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_INCOMING_CALL, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "caller")], conf)
    for conf in config.get(CONF_ON_CALL_CONNECTED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_CALL_ENDED, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DTMF, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [(cg.std_string, "digit")], conf)


SIMPLE_ACTION_SCHEMA = automation.maybe_simple_id(
    {cv.Required(CONF_ID): cv.use_id(SipClient)}
)

CALL_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(SipClient),
        cv.Required(CONF_NUMBER): cv.templatable(cv.string),
    }
)

DTMF_ACTION_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(SipClient),
        cv.Required(CONF_DIGITS): cv.templatable(cv.string),
    }
)


@automation.register_action("sip_client.call", CallAction, CALL_ACTION_SCHEMA)
async def call_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))
    templ = await cg.templatable(config[CONF_NUMBER], args, cg.std_string)
    cg.add(var.set_number(templ))
    return var


@automation.register_action("sip_client.answer", AnswerAction, SIMPLE_ACTION_SCHEMA)
async def answer_action_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))


@automation.register_action("sip_client.hangup", HangupAction, SIMPLE_ACTION_SCHEMA)
async def hangup_action_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))


@automation.register_action("sip_client.send_dtmf", SendDtmfAction, DTMF_ACTION_SCHEMA)
async def send_dtmf_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))
    templ = await cg.templatable(config[CONF_DIGITS], args, cg.std_string)
    cg.add(var.set_digits(templ))
    return var
