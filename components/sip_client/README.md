# sip_client

[Installation](#installation) · [Configuration](#configuration) · [Actions](#actions-automation) · [Device packages](../../packages/sip/README.md)

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
| `server` | ✓ | - | PBX address. An IP is recommended for `udp`; for `wss` with certificate verification use the hostname that appears in the server certificate (CN/SAN). |
| `transport` | | `udp` | Signaling transport: `udp` (plain SIP), `ws` or `wss` (SIP over WebSocket / secure WebSocket, RFC 7118; ESP32 only). See [SIP over WebSocket](#sip-over-websocket-remote-pbx). RTP audio is always plain UDP. |
| `port` | | 5060 / 8088 / 8089 | SIP server port. The default follows `transport`: `5060` for `udp`, `8088` for `ws`, `8089` for `wss` (FreePBX / Asterisk PJSIP defaults). |
| `path` | | `/ws` | WebSocket URL path (`ws` / `wss` only). |
| `verify_ssl` | | `true` | `wss` only. Verify the server certificate against the ESP-IDF CA bundle (or `ca_certificate`). `false` disables all verification (a warning is logged) and makes the connection open to interception; use it only for testing. |
| `ca_certificate` | | - | `wss` only. PEM text of the CA (or the self-signed server certificate) to trust instead of the built-in bundle, e.g. for a PBX with a self-signed certificate. Cannot be combined with `verify_ssl: false`. |
| `username` | ✓ | - | SIP account (extension) |
| `auth_username` | | `username` | Digest authentication user, for PBXs that separate it from the extension number (3CX v20 calls it *Authentication ID*). `From` / `To` / `Contact` keep using `username`, so the device still presents its extension. Omit unless the PBX rejects REGISTER with *credentials don't match*. |
| `password` | ✓ | - | SIP password |
| `domain` | | server | SIP domain / realm |
| `caller_id` | | username | Outgoing display name |
| `register_expiration` | | 300s | Registration refresh interval |
| `local_rtp_port` | | 7078 | Local UDP port the device binds for RTP audio and advertises in SDP. Usually left at the default; change it only to avoid a port clash or to pin a firewall/NAT forward. |
| `rtp_keepalive` | | `0s` | How often a silence frame is sent while there is no audio to transmit (no microphone, half-duplex listening, or a quiet start of call). `0s` keeps a continuous 20 ms silence stream (about 80 kbit/s upstream for G.711); `10s` sends continuous silence for the first ~2 s of a call and then one frame per period, which still holds the router's UDP mapping. Recommended `5s`-`15s`: keep it below the router's UDP timeout (often ~30 s) and any PBX `rtp_timeout`. Allowed: `0s` or `1s`-`60s`. See [RTP and NAT](#rtp-and-nat-remote-device). |
| `channel` | | `stereo` | How call audio is pushed to the `speaker`. `stereo` (default) duplicates the mono call audio to L/R for stereo chains (e.g. a `mixer`/`resampler` feeding a stereo DAC like the Voice PE's AIC3204). Use `mono` for a single-channel codec such as the **es8311** (whose i2s speaker is set `channel: mono` and expects 1-channel input). Must match the channel count the assigned speaker expects. To route mono audio to one physical side, leave this `mono` and use the **speaker's** own `channel: left`/`right`. |
| `half_duplex` | | `false` | Push-to-talk mode for boards that cannot capture and play at the same time (mic and speaker on a single shared I2S bus, e.g. M5Stack Atom Echo). When `true`, the mic and speaker are never active together: the call starts in **listen** (speaker) mode and you switch to **talk** (mic) with the `start_talking` / `stop_talking` actions. Requires both `microphone` and `speaker`. Leave `false` for full-duplex boards with separate input/output I2S buses (e.g. Voice PE). |
| `codecs` | | `[pcmu, pcma]` | Ordered list of audio codecs to offer and prefer during negotiation. Allowed values: `pcmu`, `pcma`, `g722`. The first codec in the list that the remote party also supports is chosen. G.722 is **opt-in** — omit it from the list (or leave the default) to stay G.711-only. Example for wideband: `codecs: [g722, pcmu, pcma]`. |

### SIP over WebSocket (remote PBX)

For a device that is not on the PBX's network, `transport: wss` carries SIP over
a TLS WebSocket (`wss://<server>:8089/ws` on FreePBX / Asterisk). The connection
is outbound and kept open, so signaling needs no port forwarding to the device;
only the RTP audio stays plain UDP (no SRTP/DTLS/ICE).

- A WebSocket ping every 20 s keeps NAT/proxy state alive; the link is dropped
  if no pong arrives within 45 s. A dropped link is reconnected automatically
  (every ~5 s, also after the PBX closes the socket; a link that stays down
  for ~3 minutes is restarted from scratch, and time without WiFi is not
  counted). An active call is ended and the device registers again.
- The Via/Contact address is a random `<token>.invalid` name per connection
  (RFC 7118), so the extension needs `rtp_symmetric`, `force_rport` and
  `rewrite_contact`.
- Each SIP message must fit in 8 KB; larger ones are dropped with a warning.
- `transport: ws` (plain, unencrypted) is for debugging only.
- Requires an ESP32 with the ESP-IDF framework (rejected otherwise).

#### FreePBX over WSS (remote device)

Menu names are approximate and differ slightly between FreePBX 16 and 17.

**1. PBX side (once)**

- *Settings > Advanced Settings*: enable the built-in HTTP server's TLS
  (`HTTPS Bind Port` 8089; "Enable TLS for the mini-HTTP Server").
- *Admin > Certificate Management*: get a valid certificate for the PBX's public
  hostname (Let's Encrypt) and set it as the default certificate, so the
  HTTP/TLS server on 8089 uses it. Restart/reload Asterisk afterwards.
- *Settings > Asterisk SIP Settings > SIP Settings [chan_pjsip]*: enable the
  `0.0.0.0-wss` transport. *External Address* and *Local Networks* are under
  *General SIP Settings > NAT Settings* (FreePBX 16/17): set the public
  IP/hostname and your LAN ranges so the SDP the PBX sends is correct.
- Router/firewall in front of the PBX: forward **TCP 8089** and **UDP 10000-20000**
  (the RTP range, *Asterisk SIP Settings > RTP Settings*) to FreePBX. If the
  FreePBX Firewall module is enabled, also allow TCP 8089 and the RTP UDP range
  from the Internet zone.

**2. Extension (PJSIP, *Advanced* tab)**

| Setting | Value |
|---------|-------|
| Transport | `0.0.0.0-wss` |
| Enable WebRTC Defaults | No (ICE, DTLS, AVPF, RTCP mux all off) |
| Media Encryption | None |
| ICE Support / AVPF / RTCP Mux | No |
| Direct Media | No |
| Rewrite Contact / RTP Symmetric / Force rport | Yes |
| Max Contacts / Remove Existing | `1` / Yes |
| Codecs | `ulaw`, `alaw`, `g722` (match `codecs:`) |
| Session Timers | Default (see note) |

Session Timers can stay at the FreePBX default. The client answers the PBX's
in-dialog re-INVITE / UPDATE refreshes, and treats the PBX as the refresher: if
no refresh arrives before the session expires (minus the RFC 4028 safety
margin) it ends the call with a BYE. The client never refreshes the session
itself. Intervals below 90 s are answered `422` with `Min-SE: 90`.

**3. ESPHome**

```yaml
sip_client:
  transport: wss
  server: pbx.example.com    # public hostname that is in the certificate
  # port: 8089               # defaults: 8089 (wss), 8088 (ws)
  # path: /ws
  # verify_ssl: true         # default; false only for testing
  # ca_certificate: |        # only for a private / self-signed CA
  #   -----BEGIN CERTIFICATE-----
  #   ...
  #   -----END CERTIFICATE-----
  username: "1001"
  password: !secret sip_password
  rtp_keepalive: 10s         # optional, saves upstream while idle (see below)
  microphone: mic_id
  speaker: spk_id
```

#### RTP and NAT (remote device)

The device advertises its private LAN IP in the SDP `c=` line. That is fine
with `rtp_symmetric` + `force_rport` + `rewrite_contact`: Asterisk ignores the
advertised address and sends RTP to the source address/port it sees on the
first packet from the device. Nothing STUN-like is used or needed. This
only works if the device sends first and keeps sending:

- RTP is sent from the same UDP socket that is bound to `local_rtp_port` and
  advertised in the SDP, so the source port the PBX sees is the one the router
  mapped.
- While there is no audio to send (receive-only build, half-duplex while
  listening, start of a call before the microphone delivers data), the device
  sends silence frames in the negotiated codec, keeping the session's
  sequence numbers, timestamps and SSRC consistent.
- With strict RTP (Asterisk default) the PBX needs a few consecutive packets
  (`probation`, default 4) from the device's public address before it switches
  to it. By default (`rtp_keepalive: 0s`) the silence is continuous (20 ms
  cadence), so this always completes.
- With `rtp_keepalive` set (`1s`-`60s`), the idle stream is thinned to one frame
  per period, but the device still sends continuous silence for the first ~2 s
  of a call (and ~0.75 s after each stretch of real audio), so latching
  completes even with some packet loss. Choose 5s-15s: below the router's UDP
  mapping timeout (many consumer routers use ~30 s) and below any
  per-extension *RTP Timeout* (`rtp_timeout`) set on the PBX, or an idle
  receive-only call may be hung up. Real audio and DTMF also refresh the mapping.

#### Troubleshooting

- **401 / 403 on REGISTER**: wrong password or `username`; for 403 check the
  extension's *Max Contacts* / the PBX's `fail2ban` list (your public IP may be
  blocked), and that the transport is `0.0.0.0-wss`.
- **One-way or no audio, call otherwise connects**: UDP 10000-20000 not
  forwarded/open to the PBX, *Direct Media* on, *RTP Symmetric* / *Force rport*
  off, or the router drops the idle UDP mapping (lower `rtp_keepalive`).
  Check the *Media started* log line and `rtp set debug on` on the PBX.
- **Certificate / TLS errors in the log**: `server` must be the hostname in
  the certificate (an IP only works with `verify_ssl: false`), the PBX must
  serve the full chain, and a private CA needs `ca_certificate`. Check
  the device clock and that TCP 8089 is reachable.
- **Registers, but the call is dropped after a reconnect**: expected; the
  device registers again on the new connection (keep *Remove Existing* = Yes).

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
  packets, do not re-fire the trigger. A 4-byte payload on any other dynamic
  payload type (96–127) is also taken as a telephone-event, for ATAs that never
  offer one in their SDP or send it on a different PT than negotiated.
- **SIP INFO**, answered with `200 OK`. Content types `application/dtmf-relay`,
  `application/dtmf` and `audio/telephone-event` are accepted, as is a body with
  no Content-Type at all. The digit is read from a `Signal=` / `d=` / `dtmf=`
  line or from a body that is nothing but the digit; `*` and `#` are accepted
  both literally and as the event codes `10` / `11`. INFO requests outside an
  established call are answered but do not fire the trigger.

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
  running by sending encoded silence at the normal 20 ms cadence (or one frame per `rtp_keepalive` period when set), as a hardware
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
- **Mid-dialog re-INVITE / UPDATE are supported** (RFC 3261 §14, RFC 3311)
  while a call is up:
  - *Session timers* (RFC 4028): the PBX refreshes with re-INVITE or UPDATE
    and the client answers `200 OK` (with `Session-Expires` / `Supported:
    timer`). The PBX has to be the refresher; if a peer insists on
    `refresher=uas` (the ESP would have to refresh) the client leaves the
    header out of its answer, like a UA without timer support. The client
    never sends a refresh of its own.
  - *Hold / unhold*: `a=sendonly` / `recvonly` / `inactive` and the legacy
    `c=0.0.0.0` form. While the peer holds the call the client stops sending
    RTP (microphone audio and the idle silence) and drops received audio, but
    keeps the RTP socket open; unhold restores both. The `o=` version of its
    SDP increases only when the SDP changes.
  - *Media re-anchoring* (Direct Media / PBX transfer): a new remote RTP
    address or port is picked up without interrupting the call. A different
    codec or an IPv4/IPv6 change restarts the audio, which leaves a brief gap.
    An offer without a common codec is refused with `488` and the call
    continues unchanged.
  - *Offer-less re-INVITE* (RFC 6337): the `200 OK` carries an offer with the
    negotiated codec and the SDP answer is taken from the `ACK`.
  - A re-INVITE before the initial call is ACKed gets `500` with `Retry-After`;
    a request for an unknown dialog gets `481`; an out-of-order CSeq gets `500`;
    a new offer while the answer to our own offer (offer-less re-INVITE) is
    still outstanding gets `491`. A body-less `UPDATE` before the call is
    confirmed (ringing or waiting for the ACK) is simply answered `200`.
  - Not covered: the device cannot initiate a transfer, hold or re-INVITE
    itself, and it does not process `REFER`.
  - Even though re-anchoring works, keep *Direct Media* **off** (FreePBX /
    Asterisk: `directmedia=no`; Grandstream and similar: Direct Media / media
    relay off) for a device behind NAT or reached over WSS from the internet:
    the ESP's RTP must reach the peer, which only works reliably when it stays
    hairpinned through the PBX.

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

## Home Assistant Integration (hass-sip)

Looking to connect your ESPHome SIP devices to Home Assistant?

Check out the Home Assistant custom component: [**hass-sip (eigger/hass-sip)**](https://github.com/eigger/hass-sip).

- **Registers Home Assistant on the PBX**: Functions as a SIP extension alongside your ESPHome endpoints.
- **Voice Assist over SIP**: Dial into Home Assistant from your ESP32 intercom or phone for multi-turn voice commands.
- **Intercom Auto-Answer & DTMF**: Automatically answer door station calls and send DTMF digits (e.g., door/gate release) from Home Assistant dashboards.
- **TTS Call Broadcasts**: Home Assistant automations can dial your ESP32 paging speakers or intercoms to deliver spoken announcements.
- **Matching Codecs**: Built with identical G.711 (PCMU/PCMA) and wideband G.722 codec support for clean audio interoperability.

