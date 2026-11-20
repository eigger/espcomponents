import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.core import CORE
from esphome.components import esp32
from esphome.components import microphone, speaker
from esphome.const import (
    CONF_ID,
    CONF_PATH,
    CONF_PORT,
    CONF_USERNAME,
    CONF_PASSWORD,
    CONF_MICROPHONE,
    CONF_SPEAKER,
    CONF_TRIGGER_ID,
)
from .const import (
    CONF_AUTH_USERNAME,
    CONF_SERVER,
    CONF_DOMAIN,
    CONF_CALLER_ID,
    CONF_REGISTER_EXPIRATION,
    CONF_LOCAL_RTP_PORT,
    CONF_RTP_KEEPALIVE,
    CONF_CA_CERTIFICATE,
    CONF_CHANNEL,
    CONF_TRANSPORT,
    CONF_VERIFY_SSL,
    CONF_HALF_DUPLEX,
    CONF_CODECS,
    CONF_ON_REGISTERED,
    CONF_ON_INCOMING_CALL,
    CONF_ON_CALL_CONNECTED,
    CONF_ON_CALL_ENDED,
    CONF_ON_DTMF,
    CONF_NUMBER,
    CONF_DIGITS,
)

# Pinned to the version ws_bridge already builds against in CI. 1.7.0 has every
# field used here (subprotocol, ping_interval_sec, pingpong_timeout_sec,
# crt_bundle_attach, cert_pem, event 'fin') and supports IDF >= 5.0.
ESP_WEBSOCKET_CLIENT_VERSION = "1.7.0"

CODEOWNERS = ["@eigger"]
DEPENDENCIES = ["network"]
AUTO_LOAD = ["socket", "md5", "audio"]

# cv.sensitive (ESPHome 2026.11+) masks the field in frontends/dumps; older
# releases lack it, so fall back to the plain validator.
_sensitive = getattr(cv, "sensitive", lambda inner: inner)

sip_client_ns = cg.esphome_ns.namespace("sip_client")
SipClient = sip_client_ns.class_("SipClient", cg.Component)

SipAudioChannel = sip_client_ns.enum("SipAudioChannel")
CHANNEL_MODES = {
    "mono": SipAudioChannel.SIP_CH_MONO,
    "stereo": SipAudioChannel.SIP_CH_STEREO,
}

AudioCodecId = sip_client_ns.enum("AudioCodecId", is_class=True)
CODEC_MODES = {
    "pcmu": AudioCodecId.PCMU,
    "pcma": AudioCodecId.PCMA,
    "g722": AudioCodecId.G722,
}

SipTransportKind = sip_client_ns.enum("SipTransportKind")
TRANSPORTS = {
    "udp": SipTransportKind.SIP_TRANSPORT_UDP,
    "ws": SipTransportKind.SIP_TRANSPORT_WS,
    "wss": SipTransportKind.SIP_TRANSPORT_WSS,
}
DEFAULT_PORTS = {"udp": 5060, "ws": 8088, "wss": 8089}

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
StartTalkingAction = sip_client_ns.class_("StartTalkingAction", automation.Action)
StopTalkingAction = sip_client_ns.class_("StopTalkingAction", automation.Action)


def validate_audio(config):
    has_mic = CONF_MICROPHONE in config
    has_spk = CONF_SPEAKER in config
    if not has_mic and not has_spk:
        raise cv.Invalid("At least one of 'microphone' or 'speaker' is required")
    # Push-to-talk switches a shared I2S bus between mic and speaker.
    if config.get(CONF_HALF_DUPLEX, False) and not (has_mic and has_spk):
        raise cv.Invalid("'half_duplex' requires both 'microphone' and 'speaker'")
    return config


def validate_rtp_keepalive(value):
    # 0 = continuous silence while idle; otherwise one frame per period, 1 s - 60 s.
    value = cv.positive_time_period_milliseconds(value)
    ms = value.total_milliseconds
    if ms != 0 and not 1000 <= ms <= 60000:
        raise cv.Invalid("'rtp_keepalive' must be 0s or between 1s and 60s")
    return value


def validate_ws_path(value):
    value = cv.string_strict(value)
    if not value.startswith("/"):
        raise cv.Invalid("'path' must start with '/'")
    return value


def validate_pem(value):
    value = cv.string_strict(value).strip()
    if "-----BEGIN CERTIFICATE-----" not in value or "-----END CERTIFICATE-----" not in value:
        raise cv.Invalid("'ca_certificate' must be a PEM encoded certificate")
    return value + "\n"


def validate_transport(config):
    transport = config[CONF_TRANSPORT]
    if transport != "udp":
        if not CORE.is_esp32:
            raise cv.Invalid(
                f"transport '{transport}' (SIP over WebSocket) is only supported on ESP32"
            )
        if CORE.target_framework != "esp-idf":
            raise cv.Invalid(
                "transport ws/wss requires the ESP-IDF framework (esp32: framework: type: esp-idf)"
            )
        config.setdefault(CONF_PATH, "/ws")
    elif CONF_PATH in config:
        raise cv.Invalid("'path' is only valid with transport 'ws' or 'wss'")
    if transport == "wss":
        config.setdefault(CONF_VERIFY_SSL, True)
        if CONF_CA_CERTIFICATE in config and not config[CONF_VERIFY_SSL]:
            raise cv.Invalid("'ca_certificate' has no effect with 'verify_ssl: false'")
    else:
        for key in (CONF_VERIFY_SSL, CONF_CA_CERTIFICATE):
            if key in config:
                raise cv.Invalid(f"'{key}' is only valid with transport 'wss'")
    config.setdefault(CONF_PORT, DEFAULT_PORTS[transport])
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SipClient),
            # No default: the microphone is genuinely optional here (receive-only
            # devices omit it). A `default={}` would run the source schema even
            # when the key is absent, and its GenerateID would then fail to find
            # a microphone to bind to.
            cv.Optional(CONF_MICROPHONE): microphone.microphone_source_schema(
                min_bits_per_sample=16,
                max_bits_per_sample=16,
                min_channels=1,
                max_channels=1,
            ),
            cv.Optional(CONF_SPEAKER): cv.use_id(speaker.Speaker),
            cv.Required(CONF_SERVER): cv.string,
            cv.Optional(CONF_TRANSPORT, default="udp"): cv.one_of("udp", "ws", "wss", lower=True),
            # Default depends on the transport (5060 / 8088 / 8089), see validate_transport.
            cv.Optional(CONF_PORT): cv.port,
            cv.Optional(CONF_PATH): validate_ws_path,
            cv.Optional(CONF_VERIFY_SSL): cv.boolean,
            cv.Optional(CONF_CA_CERTIFICATE): validate_pem,
            cv.Required(CONF_USERNAME): cv.string,
            cv.Optional(CONF_AUTH_USERNAME): cv.string,
            cv.Required(CONF_PASSWORD): _sensitive(cv.string),
            cv.Optional(CONF_DOMAIN): cv.string,
            cv.Optional(CONF_CALLER_ID): cv.string,
            cv.Optional(
                CONF_REGISTER_EXPIRATION, default="300s"
            ): cv.positive_time_period_seconds,
            cv.Optional(CONF_LOCAL_RTP_PORT, default=7078): cv.port,
            cv.Optional(CONF_RTP_KEEPALIVE, default="0s"): validate_rtp_keepalive,
            cv.Optional(CONF_CHANNEL, default="stereo"): cv.enum(CHANNEL_MODES, lower=True),
            cv.Optional(CONF_HALF_DUPLEX, default=False): cv.boolean,
            cv.Optional(CONF_CODECS, default=["pcmu", "pcma"]): cv.ensure_list(
                cv.enum(CODEC_MODES, lower=True)
            ),
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
    ).extend(cv.COMPONENT_SCHEMA),
    validate_audio,
    validate_transport,
)


def _setup_websocket(config):
    """Pull in esp_websocket_client and the TLS pieces it needs."""
    cg.add_define("USE_SIP_CLIENT_WS")
    esp32.add_idf_component(name="espressif/esp_websocket_client", ref=ESP_WEBSOCKET_CLIENT_VERSION)
    # The managed client links ESP-TLS even for plain ws://, so ESPHome's
    # trimmed mbedTLS client symbols are needed in every WS configuration
    # (same dance as ws_bridge, which supports older/newer ESPHome releases).
    if hasattr(esp32, "request_tls"):
        esp32.request_tls()
    elif hasattr(esp32, "include_builtin_idf_component"):
        esp32.include_builtin_idf_component("esp-tls")
        if hasattr(esp32, "add_idf_sdkconfig_option"):
            esp32.add_idf_sdkconfig_option("CONFIG_MBEDTLS_TLS_ENABLED", True)
    if config[CONF_TRANSPORT] != "wss":
        return
    if not config[CONF_VERIFY_SSL]:
        # Without a CA / bundle esp-tls refuses to connect unless insecure
        # mode is compiled in.
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_INSECURE", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", True)
    elif CONF_CA_CERTIFICATE not in config:
        # Default verification uses ESP-IDF's public CA bundle.
        cg.add_define("USE_SIP_CLIENT_CERT_BUNDLE")
        if hasattr(esp32, "require_certificate_bundle"):
            esp32.require_certificate_bundle()
        elif hasattr(esp32, "require_full_certificate_bundle"):
            esp32.require_full_certificate_bundle()


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_MICROPHONE in config:
        mic_source = await microphone.microphone_source_to_code(config[CONF_MICROPHONE])
        cg.add(var.set_microphone_source(mic_source))
    if CONF_SPEAKER in config:
        spk = await cg.get_variable(config[CONF_SPEAKER])
        cg.add(var.set_speaker(spk))

    cg.add(var.set_server(config[CONF_SERVER]))
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_transport(TRANSPORTS[config[CONF_TRANSPORT]]))
    if config[CONF_TRANSPORT] != "udp":
        cg.add(var.set_ws_path(config[CONF_PATH]))
        _setup_websocket(config)
    if CONF_VERIFY_SSL in config:
        cg.add(var.set_verify_ssl(config[CONF_VERIFY_SSL]))
    if CONF_CA_CERTIFICATE in config:
        cg.add(var.set_ca_certificate(config[CONF_CA_CERTIFICATE]))
    cg.add(var.set_username(config[CONF_USERNAME]))
    if CONF_AUTH_USERNAME in config:
        cg.add(var.set_auth_username(config[CONF_AUTH_USERNAME]))
    cg.add(var.set_password(config[CONF_PASSWORD]))
    if CONF_DOMAIN in config:
        cg.add(var.set_domain(config[CONF_DOMAIN]))
    if CONF_CALLER_ID in config:
        cg.add(var.set_caller_id(config[CONF_CALLER_ID]))
    cg.add(var.set_register_expiration(config[CONF_REGISTER_EXPIRATION]))
    cg.add(var.set_local_rtp_port(config[CONF_LOCAL_RTP_PORT]))
    cg.add(var.set_rtp_keepalive(config[CONF_RTP_KEEPALIVE].total_milliseconds))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_half_duplex(config[CONF_HALF_DUPLEX]))
    cg.add(var.set_codecs([CODEC_MODES[c] for c in config[CONF_CODECS]]))

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


@automation.register_action("sip_client.call", CallAction, CALL_ACTION_SCHEMA, synchronous=True)
async def call_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))
    templ = await cg.templatable(config[CONF_NUMBER], args, cg.std_string)
    cg.add(var.set_number(templ))
    return var


@automation.register_action("sip_client.answer", AnswerAction, SIMPLE_ACTION_SCHEMA, synchronous=True)
async def answer_action_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))


@automation.register_action("sip_client.hangup", HangupAction, SIMPLE_ACTION_SCHEMA, synchronous=True)
async def hangup_action_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))


@automation.register_action("sip_client.send_dtmf", SendDtmfAction, DTMF_ACTION_SCHEMA, synchronous=True)
async def send_dtmf_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))
    templ = await cg.templatable(config[CONF_DIGITS], args, cg.std_string)
    cg.add(var.set_digits(templ))
    return var


@automation.register_action("sip_client.start_talking", StartTalkingAction, SIMPLE_ACTION_SCHEMA, synchronous=True)
async def start_talking_action_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))


@automation.register_action("sip_client.stop_talking", StopTalkingAction, SIMPLE_ACTION_SCHEMA, synchronous=True)
async def stop_talking_action_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg, await cg.get_variable(config[CONF_ID]))
