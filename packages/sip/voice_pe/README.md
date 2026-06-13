# Home Assistant Voice PE — SIP Intercom

Turns a [Home Assistant Voice PE](https://www.home-assistant.io/voice-pe/) (ESP32-S3) into a **SIP intercom / phone**. It uses the [`sip_client`](../../../components/sip_client) component to register with a PBX (Asterisk/FreePBX/3CX, etc.) and carries two-way voice through the device's built-in microphone and speaker. The stock Voice PE features (LED ring, dial, media player, voice_kit) keep working.

## Features

- Registers with a PBX (REGISTER + MD5 Digest auth), then **receives and places calls**
- **Two-way voice** through the built-in mic/speaker (G.711 PCMU/PCMA, 8 kHz)
- **Center button** to dial / answer / hang up
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
  sip_destination: "101"          # number the center button dials

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents/
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
| `sip_destination` | `1001` | Number dialed by the center button |
| `hidden_ssid` | `false` | Set `true` for a hidden SSID |
| `sip_password` | — | **Not a substitution → `sip_password` in `secrets.yaml`** |

## Behavior

### Center button (short press)
The action depends on the current call state.

| State | Action |
|-------|--------|
| Idle (not muted) | **Dial** `sip_destination` |
| Ringing (incoming) | **Answer** the call |
| In call / dialing | **Hang up** |

> Double / triple / long presses are independent of calls and are exposed as the `Button press` event entity for Home Assistant automations.

### LED ring
The `control_leds` script reflects the call state.

| State | LED |
|-------|-----|
| Ringing | Green rotating effect (`Incoming Call`) |
| In call | Solid cyan (`In Call`) |
| Idle | Default (user's LED ring setting) |

### Ringtone
On an incoming call a ringtone is played (looped) through the media player as an announcement, and stopped when the call connects or ends.

### Triggers (for automations)
The package wires up the `sip_client` triggers: `on_registered`, `on_incoming_call` (variable `caller`), `on_call_connected`, and `on_call_ended`. Extend them in your device YAML if needed.

## Requirements / Notes

- A reachable **SIP PBX** and a registered account are required.
- Codec is **G.711 (PCMU/PCMA, 8 kHz)** — make sure the PBX allows it.
- **Keep IPv6 disabled.** The `network: enable_ipv6` line in the package is commented out (registration fails with IPv6 enabled; a component-side fix is planned).
- Use an **IP address** for the server (hostname DNS resolution is not supported).
- Only one call at a time is supported.
