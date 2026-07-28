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
- **Home Assistant control**: a `SIP number to dial` text entity plus
  `SIP call` / `SIP hang up` buttons, so calls can be placed and ended from
  HA (dashboards, automations) as well as from the device
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
| `sip_destination` | `1001` | Initial value of the `SIP number to dial` entity (used by the long press and the `SIP call` button) |
| `hidden_ssid` | `false` | Set `true` for a hidden SSID |
| `sip_password` | — | **Not a substitution → `sip_password` in `secrets.yaml`** |

## Behavior

### Center button

| Press | State | Action |
|-------|-------|--------|
| Short | Ringing (incoming) | **Answer** the call |
| Short | In call / dialing | **Hang up** |
| Short | Idle | Original stock behavior (abort timer/announcement/music, or start the voice assistant) |
| Long | Idle | **Dial** the `SIP number to dial` entity |
| Long | Ringing / in call | Original stock behavior (`Button press` event, `long_press`) |

> Double / triple presses are unchanged from stock and are exposed as the
> `Button press` event entity for Home Assistant automations.

### Home Assistant entities

| Entity | Type | Purpose |
|--------|------|---------|
| `SIP number to dial` | text (config) | Number to call. Defaults to `sip_destination`; also what the center button's long press dials |
| `SIP call` | button | Place a call to the number above |
| `SIP hang up` | button | Decline a ringing call, cancel a pending outgoing one, or end an active call (no-op when idle) |
| `Decline calls during voice assistant` | switch (config) | See below — off by default |

### Voice assistant / wake word interaction

By default, an incoming or active call takes priority over the voice
assistant: if the assistant is running when a call comes in, is answered, or
is dialed out, it's stopped (along with `micro_wake_word`, so the two never
compete for the microphone), and wake-word detection stays off for the
duration of the call. Once the call ends, wake word and the voice assistant
resume working normally.

Flip it around with the **"Decline calls during voice assistant"** switch
(off by default, `entity_category: config`): while it's on, an incoming call
that arrives while the voice assistant is actively running an Assist turn is
auto-declined (SIP `603 Decline`) instead of ringing and interrupting it. A
call that arrives while the assistant is idle (just listening for the wake
word) still rings normally either way — this only affects calls that land
mid-conversation.

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
- **IPv6 is disabled** (`network: enable_ipv6: false`). `sip_client` now picks
  its UDP socket family from the actual server/peer address rather than this
  global setting, so it should work either way — this is left off mainly
  because IPv6 SIP servers (bracketed literals in Via/Contact/SDP) aren't
  supported yet.
- Use an **IP address** for the server (hostname DNS resolution is not
  supported).
- Only one call at a time is supported.
- `ota:` and the `restart` button are intentionally **not** defined here —
  `packages/device_base.yaml` already provides OTA (with the password) and a
  restart switch; redefining them here would either collide with or duplicate
  those.

## Troubleshooting

### Adding your own wake word models *adds to* the built-in ones

A `micro_wake_word:` block in your device YAML does **not** replace the
package's models — ESPHome merges the `models:` lists, so your entries are
appended to the four this package already ships (`okay_nabu`, `hey_jarvis`,
`hey_mycroft`, and the internal `stop`).

This mostly costs **flash**, not CPU: every model is embedded in the firmware,
but only *enabled* models are loaded and run inference. ESPHome enables just
the first model by default and persists each model's enabled state to flash,
so the rest stay unloaded until you turn them on — you pick which ones from
the wake-word controls Home Assistant exposes for the device.

So if the device feels overloaded, check **how many wake words you enabled in
HA**, not how many are listed in the YAML. Each enabled model allocates its
own tensor arena and runs per-frame inference, on a device that is also doing
Assist, media playback and (with this package) SIP audio.

### `stt-stream-failed` / silent replies

`Error: stt-stream-failed - speech-to-text failed` comes from the Home
Assistant Assist pipeline, not from `sip_client` — the device could not
stream microphone audio to HA for speech-to-text. It's almost always network
or CPU, not the SIP component:

- **Check `output_power`.** ESPHome allows `8.5dB` – `20.5dB`; `8.5dB` is the
  *minimum*, which cripples the device's uplink even when the signal it
  *receives* (`WiFi Signal`, e.g. `-49 dBm`) looks great. STT streaming is
  uplink-heavy, so leave `output_power` at the default unless you have a
  specific reason.
- **Don't use `power_save_mode: HIGH`.** This package sets `NONE` on purpose;
  WiFi power saving adds latency and packet loss that real-time audio can't
  absorb.
- **Watch how many wake words are enabled** (above) and `Heap Frei` /
  `Heap Max Block` in the `debug:` sensors. Internal heap dropping toward
  ~45 KB with a max block around ~37 KB means memory pressure, and task
  allocations start failing.

### Reading a crash backtrace

ESPHome already decodes the backtrace in the log output (the
`WARNING Decoded 0x...` lines) as long as the build directory for that exact
firmware is still present — no manual `addr2line` needed. If you do want to
run it yourself, the ELF is at:

```
<config dir>/.esphome/build/<device name>/.pioenvs/<device name>/firmware.elf
```

Note that a backtrace ending in `esp_cpu_wait_for_intr` / `prvIdleTask` is the
**idle task**, i.e. it tells you nothing about the real fault — that pattern
usually means a task watchdog fired on the other core. In that case, look at
`Reset:` in the device-info text sensor (`task watchdog` vs. a real panic)
rather than at the backtrace.
