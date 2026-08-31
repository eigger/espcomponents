# sip_client

An ESPHome external component for ESP32. It registers to a SIP PBX
(Asterisk/FreePBX/3CX, etc.) and makes/receives voice calls. Audio I/O is
delegated to ESPHome's standard `microphone` / `speaker` platforms (either may
be omitted for send-only or receive-only endpoints). Codecs are G.711
(PCMU/PCMA, 8 kHz) by default, with optional wideband **G.722** (16 kHz PCM,
8 kHz RTP clock per RFC 3551). DTMF is sent via RFC 2833 (telephone-event) and
received via RFC 2833 or SIP INFO.

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
  microphone: mic_id        # omit for receive-only (speaker / announcements)
  speaker: spk_id           # omit for send-only (mic / uplink)
  server: 192.168.0.10      # PBX address (IP recommended)
  port: 5060                # (default 5060)
  username: "1001"
  auth_username: "auth-id"  # (optional) Digest auth ID, when the PBX separates it
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

At least one of `microphone` / `speaker` is required. SDP advertises the matching
direction: both → `sendrecv`, speaker only → `recvonly`, microphone only → `sendonly`.

```yaml
# Receive-only (announcement / paging speaker, no mic)
sip_client:
  id: my_sip
  speaker: spk_id
  # ...

# Send-only (uplink mic, no speaker)
sip_client:
  id: my_sip
  microphone: mic_id
  # ...
```

### Options

| Option | Required | Default | Description |
|--------|:--------:|---------|-------------|
| `microphone` | | - | Microphone to capture from. Either an ID (`microphone: mic_id`) or a [microphone source](#microphone-source) block for per-channel/gain control. Omit for receive-only devices. |
| `speaker` | | - | ID of the speaker component to use. Omit for send-only devices. |
| `server` | ✓ | - | PBX address (IP recommended) |
| `port` | | 5060 | SIP server port |
| `username` | ✓ | - | SIP account (extension) |
| `auth_username` | | `username` | Digest authentication user, for PBXs that separate it from the extension number (3CX v20 calls it *Authentication ID*). `From` / `To` / `Contact` keep using `username`, so the device still presents its extension. Omit unless the PBX rejects REGISTER with *credentials don't match*. |
| `password` | ✓ | - | SIP password |
| `domain` | | server | SIP domain / realm |
| `caller_id` | | username | Outgoing display name |
| `register_expiration` | | 300s | Registration refresh interval |
| `local_rtp_port` | | 7078 | Local UDP port the device binds for RTP audio and advertises in SDP. Usually left at the default; change it only to avoid a port clash or to pin a firewall/NAT forward. |
| `channel` | | `stereo` | How call audio is pushed to the `speaker`. `stereo` (default) duplicates the mono call audio to L/R for stereo chains (e.g. a `mixer`/`resampler` feeding a stereo DAC like the Voice PE's AIC3204). Use `mono` for a single-channel codec such as the **es8311** (whose i2s speaker is set `channel: mono` and expects 1-channel input). Must match the channel count the assigned speaker expects. To route mono audio to one physical side, leave this `mono` and use the **speaker's** own `channel: left`/`right`. |
| `half_duplex` | | `false` | Push-to-talk mode for boards that cannot capture and play at the same time (mic and speaker on a single shared I2S bus, e.g. M5Stack Atom Echo). When `true`, the mic and speaker are never active together: the call starts in **listen** (speaker) mode and you switch to **talk** (mic) with the `start_talking` / `stop_talking` actions. Requires both `microphone` and `speaker`. Leave `false` for full-duplex boards with separate input/output I2S buses (e.g. Voice PE). |
| `codecs` | | `[pcmu, pcma]` | Ordered list of audio codecs to offer and prefer during negotiation. Allowed values: `pcmu`, `pcma`, `g722`. The first codec in the list that the remote party also supports is chosen. G.722 is **opt-in** — omit it from the list (or leave the default) to stay G.711-only. Example for wideband: `codecs: [g722, pcmu, pcma]`. |

### Microphone source

`microphone:` accepts either a plain ID or ESPHome's
[microphone source](https://esphome.io/components/microphone/) block, which adds
channel selection and gain on top of the microphone component:

```yaml
sip_client:
  microphone:
    microphone: mic_id
    channels: 1       # which channel of the mic to capture (default 0)
    gain_factor: 4    # digital gain, 1-64 (default 1)
  # ...
```

Useful for stereo microphone hardware where only one channel carries the voice
(the Voice PE's dual mics, for example), or for a quiet mic that needs a boost
before it reaches the codec. Call audio is mono, so exactly one channel is
captured; `bits_per_sample` is fixed at 16.

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

# Push-to-talk (only when half_duplex: true) — switch the shared bus to the mic
- sip_client.start_talking: my_sip   # stop speaker, start mic (transmit)
- sip_client.stop_talking: my_sip    # stop mic, start speaker (receive)
```

### Push-to-talk (half-duplex) example

For a single-I2S-bus board (mic + speaker share one peripheral, e.g. Atom Echo),
hold a button to talk and release to listen:

```yaml
sip_client:
  id: my_sip
  half_duplex: true
  # channel: keep this matching your i2s speaker's `channel:` setting.
  # The stock ESPHome Atom Echo speaker uses `channel: stereo`, so leave the
  # default (stereo); use `mono` only if your speaker is set `channel: mono`.
  # ...

binary_sensor:
  - platform: gpio
    pin: GPIO39          # Atom Echo center button
    on_press:
      - sip_client.start_talking: my_sip
    on_release:
      - sip_client.stop_talking: my_sip
```

> The stock ESPHome [Atom Echo voice-assistant config](https://github.com/esphome/wake-word-voice-assistants/blob/main/m5stack-atom-echo/m5stack-atom-echo.yaml)
> puts the mic (`pdm`, din `GPIO23`) and speaker (dout `GPIO22`) on one shared
> I2S bus (LRCLK `GPIO33` / BCLK `GPIO19`) and itself stops the mic before
> playing audio — i.e. it is half-duplex. `half_duplex: true` mirrors exactly
> that behavior for calls.

## Triggers

- `on_registered` — successfully registered to the PBX
- `on_incoming_call` — incoming call (variable `caller`: `std::string`)
- `on_call_connected` — call has been connected
- `on_call_ended` — call has ended
- `on_dtmf` — DTMF received from the remote party (variable `digit`: `std::string`).
  See [DTMF reception](#dtmf-reception) for what is accepted.

### DTMF reception

`on_dtmf` fires for both of the out-of-band ways a phone can signal a digit:

- **RFC 2833 / 4733 telephone-event** (preferred, negotiated in SDP). The digit
  is reported once when the event starts, independent of the RTP marker bit —
  Yealink DECT handsets (W70B/W73H) and the 3CX Android app send the event
  without it. Repeat packets of the same event, including the retransmitted end
  packets, do not re-fire the trigger.
- **SIP INFO** with `application/dtmf-relay` (`Signal=1`) or `application/dtmf`,
  answered with `200 OK`. `*` and `#` are accepted both literally and as the
  event codes `10` / `11`. INFO requests outside an established call are
  answered but do not fire the trigger.

In-band audio tones (no telephone-event, no INFO) are **not** decoded.

## Behavior / Limitations

- Codecs: **G.711 µ-law (PCMU) / A-law (PCMA)**, 8 kHz mono, plus optional
  **G.722** wideband (16 kHz PCM, 64 kb/s). Configure preference with the
  `codecs` option (default `[pcmu, pcma]`). Both configured codecs are offered in
  SDP and the first mutually supported codec is used.
- **G.722 SDP note:** RTP advertises `G722/8000` (8 kHz clock per RFC 3551),
  not 16000. PCM capture/playback runs at 16 kHz when G.722 is active. A
  **16 kHz microphone** is recommended for G.722 calls; 8 kHz mics are upsampled.
- Media direction follows the configured endpoints (`sendrecv` / `recvonly` /
  `sendonly`). A device with only a speaker can join calls or paging as a
  listen-only endpoint; a mic-only device can uplink audio without local
  playback.
- **Silence TX:** whenever there is nothing to transmit — no microphone
  configured, or half-duplex while listening — the device keeps the RTP stream
  running by sending encoded silence at the normal 20 ms cadence, as a hardware
  SIP phone does. This is what makes receive-only work in practice: with no
  outbound packets at all, a NAT/firewall never opens the return path for the
  RTP port, and PBXs that latch onto the source address (symmetric RTP /
  `comedia`, e.g. Asterisk's `rtp_symmetric`) never learn where to send audio,
  so the *inbound* direction stays silent too. Continuous TX also satisfies
  setups with an aggressive `rtp_timeout`. Live audio is never affected: a brief
  microphone underrun waits for the real samples instead of being spliced with
  silence. Silence is produced by encoding zeroed PCM through the negotiated
  codec, which keeps G.722's stateful ADPCM encoder in step with the far-end
  decoder. RFC 3389 comfort noise (payload type 13) is not offered in SDP.
- If the microphone runs at 16 kHz it is automatically downsampled to the codec
  rate (8 kHz for G.711, 16 kHz for G.722); the speaker is configured to play at
  the active codec's PCM rate. Integer 2× rate conversion (8↔16 kHz) is handled
  internally when needed.
- **Mono codecs (e.g. es8311):** set `channel: mono` and configure the i2s
  speaker with `channel: mono`. To avoid forcing the codec's I2S clock to 8 kHz
  (which can fight a DAC initialised at a fixed rate / MCLK), feed the call audio
  through a `resampler` speaker that outputs the codec's native rate:

  ```yaml
  speaker:
    - platform: i2s_audio
      id: es8311_speaker
      channel: mono
      sample_rate: 16000        # matches es8311 audio_dac default
      bits_per_sample: 16bit
      dac_type: external
      i2s_dout_pin: GPIOxx
      audio_dac: es8311_dac
    - platform: resampler
      id: sip_resampling_speaker
      output_speaker: es8311_speaker
      sample_rate: 16000
      bits_per_sample: 16

  sip_client:
    speaker: sip_resampling_speaker
    channel: mono
    # ...
  ```

  Note: a normal call is full-duplex (mic + speaker run together), which needs
  separate input/output I2S buses. On boards that share one I2S peripheral for
  capture and playback (e.g. M5Stack Atom Echo) simultaneous record + play may
  not work — set `half_duplex: true` and drive `start_talking` / `stop_talking`
  from a button (see the Push-to-talk example above).
- **Stereo DACs (e.g. Home Assistant Voice PE / aic3204):** keep `channel: stereo`
  (the default). Route the call audio through a `resampler` into the same `mixer`
  that feeds the stereo DAC, so SIP shares the output with media/announcements:

  ```yaml
  speaker:
    - platform: i2s_audio
      id: i2s_audio_speaker
      channel: stereo
      sample_rate: 48000
      bits_per_sample: 32bit
      dac_type: external
      i2s_dout_pin: GPIO10
      audio_dac: aic3204_dac
    - platform: mixer
      id: mixing_speaker
      output_speaker: i2s_audio_speaker
      num_channels: 2
      source_speakers:
        - id: media_mixing_input
        - id: sip_mixing_input      # dedicated input so calls never corrupt TTS
    - platform: resampler
      id: sip_resampling_speaker
      output_speaker: sip_mixing_input
      sample_rate: 48000
      bits_per_sample: 16

  sip_client:
    speaker: sip_resampling_speaker
    channel: stereo                 # default; matches the 2-channel mixer
    # ...
  ```
- Registration uses **REGISTER with MD5 Digest authentication** (RFC 2617,
  qop=auth supported).
- The server address should be an **IP** (hostname DNS resolution is not
  supported).
- Designed for a PBX (registrar/proxy) scenario. Pure peer-to-peer (server-less
  direct calls) is out of scope.
- Only one call at a time is supported.
- **No mid-dialog re-INVITE.** An inbound `INVITE` is accepted only in the
  registered (idle) state; while a call is up the client answers `486 Busy
  Here`. That means call hold, unhold, and PBX **Direct Media** / re-INVITE
  session refreshes that renegotiate SDP are not supported. Keep the ESP
  extension on the PBX with Direct Media **disabled** (FreePBX / Asterisk:
  `directmedia=no`; Grandstream and similar: Direct Media / media relay off)
  so RTP stays hairpinned through the PBX for the whole call.

## Third-party codecs

G.722 support uses a vendored copy of [sippy/libg722](https://github.com/sippy/libg722)
at commit [`4c2e79c5cbcb5ee12a97a16002a073ac83396480`](https://github.com/sippy/libg722/commit/4c2e79c5cbcb5ee12a97a16002a073ac83396480).

- **License:** Carnegie Mellon (1993) and Steve Underwood (2005, public domain
  contributions) portions, plus Sippy Software BSD-style terms. See
  `components/sip_client/LIBG722_LICENSE` in this repository for the full text.
- **Local changes:**
  - `#include <string.h>` in `g722_encode.c` / `g722_decode.c` (not upstream at
    the vendored commit) for ESP-IDF/newlib builds.
  - `G722_INTERNAL` guards around the bodies of `g722_private.h` /
    `g722_common.h`, defined only in those `.c` files. ESPHome's generated
    `esphome.h` auto-includes every component `.h`; without the guard the
    real `G722_*_CTX` struct typedefs conflict with the opaque `void`
    typedefs in the public encoder/decoder headers, and `block4()` sees an
    incomplete `struct g722_band`.

## Testing with Asterisk

Create an extension (e.g. 1001) on the PBX and register with the config above;
it will show up as online in `sip show peers` (or `pjsip show endpoints`).
Calling 1001 from another SIP endpoint fires `on_incoming_call`; answering with
`sip_client.answer` opens two-way audio.
