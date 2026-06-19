# M5Stack Atom Echo — SIP Intercom (Half-Duplex)

Turns an [M5Stack Atom Echo](https://docs.m5stack.com/en/atom/atomecho) into a **SIP intercom** using the [`sip_client`](../../../components/sip_client) component. Mic and speaker share one I2S bus, so calls run in **half-duplex push-to-talk** mode (`half_duplex: true`).

Hardware pinout and audio setup follow the official [Atom Echo voice-assistant config](https://github.com/esphome/wake-word-voice-assistants/blob/main/m5stack-atom-echo/m5stack-atom-echo.yaml).

## Features

- SIP registration (REGISTER + MD5 Digest) with a PBX (Asterisk/FreePBX/3CX, etc.)
- **Half-duplex voice** — listen by default; **hold the button** to talk (PTT)
- **Short button press** — dial / answer / hang up
- LED status (WiFi, ringing, in-call, PTT)
- Ringtone on incoming calls

## Configuration

Example device YAML:

```yaml
substitutions:
  name: "esp-atom-echo-sip"
  friendly_name: "Atom Echo SIP"
  sip_server: "192.168.0.245"
  sip_username: "103"
  sip_domain: "192.168.0.245"
  sip_destination: "101"

packages:
  remote:
    refresh: always
    url: https://github.com/eigger/espcomponents@latest
    files:
      - packages/device_base.yaml
      - packages/esp32.yaml
      - packages/sip/atom_echo/atom_echo.yaml
```

Your `secrets.yaml` must provide:

```yaml
wifi_ssid: "..."
wifi_password: "..."
ota_password: "..."
sip_password: "..."
```

### Substitutions

| Key | Default | Description |
|-----|---------|-------------|
| `sip_server` | `192.168.1.10` | PBX address (IP recommended) |
| `sip_port` | `5060` | SIP server port |
| `sip_username` | `1000` | SIP account (extension) |
| `sip_domain` | `192.168.1.10` | SIP domain / realm |
| `sip_caller_id` | `ESP Atom Echo` | Outgoing display name |
| `sip_destination` | `1001` | Number dialed on short press (idle) |
| `hidden_ssid` | `false` | Set `true` for a hidden SSID |
| `ringtone_sound_file` | timer_finished.wav URL | Incoming-call ringtone |
| `sip_password` | — | **`sip_password` in `secrets.yaml`** |

## Button

| Gesture | Idle | Ringing | In call |
|---------|------|---------|---------|
| **Short press** | Dial `sip_destination` | Answer | Hang up |
| **Hold (≥150 ms)** | — | — | **Push-to-talk** (transmit) |
| **Hold 10 s** | Factory reset | Factory reset | Factory reset |

While in a call, a quick tap hangs up; holding the button switches the shared I2S bus from speaker to mic until you release.

## LED

| State | Color / effect |
|-------|----------------|
| Boot / waiting for HA | Blue slow pulse (WiFi connected) or warm white |
| No WiFi | Solid red |
| Incoming call | Green fast pulse |
| In call (listening) | Cyan |
| In call (PTT held) | Orange |

## Notes

- Codec: **G.711 (PCMU/PCMA, 8 kHz)** — ensure your PBX allows it.
- Use an **IP address** for `sip_server` (hostname DNS is not supported).
- Only one call at a time.
- `half_duplex: true` is required on Atom Echo; full-duplex is not possible on the shared I2S bus.
