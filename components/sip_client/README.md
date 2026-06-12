# sip_client

An ESPHome external component for ESP32. It registers to a SIP PBX
(Asterisk/FreePBX/3CX, etc.) and makes/receives voice calls. Audio I/O is
delegated to ESPHome's standard `microphone` / `speaker` platforms, and the
codec is G.711 (PCMU/PCMA, 8 kHz). DTMF is sent via RFC 2833 (telephone-event).

## Installation

```yaml
external_components:
  - source: github://eigger/espcomponents@latest
    components: [ sip_client ]
    refresh: always
```

## Configuration

```yaml
# Microphone/speaker can be any ESPHome platform (e.g. i2s_audio)
i2s_audio:
  - id: i2s_in
    i2s_lrclk_pin: GPIO25
    i2s_bclk_pin: GPIO26
  - id: i2s_out
    i2s_lrclk_pin: GPIO27
    i2s_bclk_pin: GPIO14

microphone:
  - platform: i2s_audio
    id: mic_id
    i2s_audio_id: i2s_in
    adc_type: external
    i2s_din_pin: GPIO33
    pdm: false

speaker:
  - platform: i2s_audio
    id: spk_id
    i2s_audio_id: i2s_out
    dac_type: external
    i2s_dout_pin: GPIO22

sip_client:
  id: my_sip
  microphone: mic_id
  speaker: spk_id
  server: 192.168.0.10      # PBX address (IP recommended)
  port: 5060                # (default 5060)
  username: "1001"
  password: "secret"
  domain: "192.168.0.10"    # (default: server)
  caller_id: "ESP Doorbell" # (optional)
  register_expiration: 300s # (default 300s)
  local_rtp_port: 7078      # (default 7078)

  on_registered:
    - logger.log: "SIP registered"
  on_incoming_call:
    - logger.log:
        format: "Incoming call from %s"
        args: [caller.c_str()]
    # Auto-answer example
    - sip_client.answer: my_sip
  on_call_connected:
    - logger.log: "Call connected"
  on_call_ended:
    - logger.log: "Call ended"
  on_dtmf:
    - logger.log:
        format: "Received DTMF %s"
        args: [digit.c_str()]
```

### Options

| Option | Required | Default | Description |
|--------|:--------:|---------|-------------|
| `microphone` | ✓ | - | ID of the microphone component to use |
| `speaker` | ✓ | - | ID of the speaker component to use |
| `server` | ✓ | - | PBX address (IP recommended) |
| `port` | | 5060 | SIP server port |
| `username` | ✓ | - | SIP account (extension) |
| `password` | ✓ | - | SIP password |
| `domain` | | server | SIP domain / realm |
| `caller_id` | | username | Outgoing display name |
| `register_expiration` | | 300s | Registration refresh interval |
| `local_rtp_port` | | 7078 | Local UDP port the device binds for RTP audio and advertises in SDP. Usually left at the default; change it only to avoid a port clash or to pin a firewall/NAT forward. |

## Actions (Automation)

```yaml
# Place an outgoing call
- sip_client.call:
    id: my_sip
    number: "200"

# Answer an incoming call
- sip_client.answer: my_sip

# End the call (IN_CALL: BYE / dialing: CANCEL / ringing: decline)
- sip_client.hangup: my_sip

# Send DTMF during a call
- sip_client.send_dtmf:
    id: my_sip
    digits: "1234#"
```

## Triggers

- `on_registered` — successfully registered to the PBX
- `on_incoming_call` — incoming call (variable `caller`: `std::string`)
- `on_call_connected` — call has been connected
- `on_call_ended` — call has ended
- `on_dtmf` — DTMF received from the remote party (variable `digit`: `std::string`)

## Behavior / Limitations

- Codec: **G.711 µ-law (PCMU) / A-law (PCMA)**, 8 kHz mono. Both are offered in
  SDP and the codec chosen by the PBX is used.
- If the microphone runs at 16 kHz it is automatically downsampled to 8 kHz; the
  speaker is configured to play at 8 kHz.
- Registration uses **REGISTER with MD5 Digest authentication** (RFC 2617,
  qop=auth supported).
- The server address should be an **IP** (hostname DNS resolution is not
  supported).
- Designed for a PBX (registrar/proxy) scenario. Pure peer-to-peer (server-less
  direct calls) is out of scope.
- Only one call at a time is supported.

## Testing with Asterisk

Create an extension (e.g. 1001) on the PBX and register with the config above;
it will show up as online in `sip show peers` (or `pjsip show endpoints`).
Calling 1001 from another SIP endpoint fires `on_incoming_call`; answering with
`sip_client.answer` opens two-way audio.
