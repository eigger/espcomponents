# Home Assistant Voice PE — Voice Assistant + SIP Intercom

Adds SIP intercom / phone functionality on top of the **full stock
[Home Assistant Voice PE](https://www.home-assistant.io/voice-pe/)** firmware
(wake word, Assist voice pipeline, timers, media player, LED ring, dial,
`voice_kit`, etc. all still work exactly as upstream). It uses the
[`sip_client`](../../../components/sip_client) component to register with a
PBX (Asterisk/FreePBX/3CX, etc.) and carries two-way voice through the
device's built-in microphone and speaker, sharing the device without
disabling anything else.

This package is kept in sync with upstream
[`esphome/home-assistant-voice-pe`](https://github.com/esphome/home-assistant-voice-pe/blob/dev/home-assistant-voice.yaml)
— SIP is layered in additively rather than replacing any stock behavior.

## Features

- Everything from the stock Voice PE firmware: wake word (`micro_wake_word`),
  the Assist voice pipeline, timers, media player, LED ring animations, dial,
  mute switch, jack detection, `voice_kit`, etc.
- Registers with a PBX (REGISTER + MD5 Digest auth), then **receives and
  places calls**
- **Two-way voice** through the built-in mic/speaker (G.711 PCMU/PCMA, 8 kHz),
  on its own dedicated audio pipeline (mixer input + resampler) so calls never
  interfere with announcements or media playback
- **Center button**: short press answers/hangs up a call (falls through to
  the normal voice-assistant toggle when idle); long press places an outgoing
  call when idle (falls through to the normal long-press event otherwise)
- An incoming/active call **stops the voice assistant** and **suppresses wake
  word detection**, so the two features never fight over the microphone
- **LED ring** indication per call state, with a **ringtone** on incoming calls
- DTMF (RFC 2833) sending via the `sip_client.send_dtmf` action

## Configuration

Example device YAML — set only the values you need to change in `substitutions`:

```yaml
substitutions:
  name: "esp-voice-pe"
  friendly_name: "ESP Voice PE"
  sip_server: "192.168.0.245"     # PBX address (IP recommended)
  sip_username: "103"             # SIP account / extension
  sip_domain: "192.168.0.245"     # usually the same as sip_server
  sip_destination: "101"          # number the center button (long press) dials

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents@latest
    files:
      - packages/device_base.yaml
      - packages/esp32.yaml
      - packages/sip/voice_pe/voice_pe.yaml
```

Your `secrets.yaml` must provide:

```yaml
wifi_ssid: "..."
wifi_password: "..."
ota_password: "..."
sip_password: "..."   # SIP password
```

### Substitutions

| Key | Default | Description |
|-----|---------|-------------|
| `name` / `friendly_name` | `esp-voice-pe` / `ESP Voice PE` | Device name |
| `sip_server` | `192.168.1.10` | PBX address (IP recommended) |
| `sip_port` | `5060` | SIP server port |
| `sip_username` | `1000` | SIP account (extension) |
| `sip_domain` | `192.168.1.10` | SIP domain / realm (usually same as server) |
| `sip_caller_id` | `ESP Voice PE` | Outgoing display name |
| `sip_destination` | `1001` | Number dialed by the center button (long press) |
| `hidden_ssid` | `false` | Set `true` for a hidden SSID |
| `sip_password` | — | **Not a substitution → `sip_password` in `secrets.yaml`** |

## Behavior

### Center button

| Press | State | Action |
|-------|-------|--------|
| Short | Ringing (incoming) | **Answer** the call |
| Short | In call / dialing | **Hang up** |
| Short | Idle | Original stock behavior (abort timer/announcement/music, or start the voice assistant) |
| Long | Idle | **Dial** `sip_destination` |
| Long | Ringing / in call | Original stock behavior (`Button press` event, `long_press`) |

> Double / triple presses are unchanged from stock and are exposed as the
> `Button press` event entity for Home Assistant automations.

### Voice assistant / wake word interaction

An incoming or active call takes priority over the voice assistant: if the
assistant is running when a call comes in or is answered, it's stopped, and
wake-word detection is ignored for the duration of the call. Once the call
ends, wake word and the voice assistant resume working normally.

### LED ring

The `control_leds` script reflects the call state, layered into the same
priority chain as the stock LED states (wake word phases, timers, mute, etc.):

| State | LED |
|-------|-----|
| Ringing | Green rotating effect (`Incoming Call`) |
| In call | Solid cyan (`In Call`) |
| Idle | Falls through to the normal stock LED state |

### Ringtone

On an incoming call a ringtone is played (looped) through the media player as
an announcement, and stopped when the call connects or ends.

### Triggers (for automations)

The package wires up the `sip_client` triggers: `on_registered`,
`on_incoming_call` (variable `caller`), `on_call_connected`, and
`on_call_ended`. Extend them in your device YAML if needed.

## Requirements / Notes

- A reachable **SIP PBX** and a registered account are required.
- Codec is **G.711 (PCMU/PCMA, 8 kHz)** — make sure the PBX allows it.
- **IPv6 is disabled** (`network: enable_ipv6: false`) — `sip_client` has a
  known IPv6 connect bug; a component-side fix is planned.
- Use an **IP address** for the server (hostname DNS resolution is not
  supported).
- Only one call at a time is supported.
- `ota:` and the `restart` button are intentionally **not** defined here —
  `packages/device_base.yaml` already provides OTA (with the password) and a
  restart switch; redefining them here would either collide with or duplicate
  those.
