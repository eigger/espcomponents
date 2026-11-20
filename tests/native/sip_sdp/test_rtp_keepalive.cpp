#include "rtp_keepalive.h"

#include <cstdint>
#include <iostream>

using esphome::sip_client::RtpKeepalive;

namespace {
int g_failures = 0;
void require_true(bool cond, const char *message) {
  if (!cond) {
    std::cerr << "FAIL: " << message << "\n";
    g_failures++;
  }
}
}  // namespace

int main() {
  // period 0: every idle slot sends (continuous silence, the legacy behaviour).
  {
    RtpKeepalive k;
    k.reset(1000);
    require_true(!k.thinned(), "default is not thinned");
    bool all = true;
    for (uint32_t t = 1000; t < 6000; t += 20) all = all && k.idle_frame_due(t);
    require_true(all, "period 0 sends every slot");
  }
  // thinned: continuous for the start burst, then one frame per period.
  {
    RtpKeepalive k;
    k.configure(10000);
    k.reset(0);
    require_true(k.thinned(), "thinned when period set");
    bool all = true;
    for (uint32_t t = 0; t < RtpKeepalive::START_BURST_MS; t += 20) {
      all = all && k.idle_frame_due(t);
      k.on_sent(t);
    }
    require_true(all, "every slot due during the start burst");
    const uint32_t end = RtpKeepalive::START_BURST_MS;
    const uint32_t last = end - 20;
    require_true(!k.idle_frame_due(end), "no frame right after the burst");
    require_true(!k.idle_frame_due(last + 9000), "no frame before the period elapsed");
    require_true(k.idle_frame_due(last + 10000), "frame due once the period elapsed");
    k.on_sent(last + 10000);
    require_true(!k.idle_frame_due(last + 10020), "period restarts after a send");
    require_true(k.idle_frame_due(last + 20000), "second period frame");
  }
  // real audio re-arms a (shorter) burst and defers the next keepalive.
  {
    RtpKeepalive k;
    k.configure(5000);
    k.reset(0);
    require_true(!k.idle_frame_due(RtpKeepalive::START_BURST_MS + 500), "drained");
    k.on_audio(10000);
    require_true(k.idle_frame_due(10000 + RtpKeepalive::AUDIO_BURST_MS - 20), "burst after audio");
    require_true(!k.idle_frame_due(10000 + RtpKeepalive::AUDIO_BURST_MS), "audio burst is finite");
    require_true(!k.idle_frame_due(14900), "period counts from last audio");
    require_true(k.idle_frame_due(15000), "period elapsed after audio");
  }
  // millis() wrap.
  {
    RtpKeepalive k;
    k.configure(10000);
    k.reset(0xFFFFFF00u);
    require_true(k.idle_frame_due(0x100u), "wrap: still inside the start burst");
    uint32_t after = 0xFFFFFF00u + RtpKeepalive::START_BURST_MS + 100;
    require_true(!k.idle_frame_due(after), "wrap: burst over");
    require_true(k.idle_frame_due(0xFFFFFF00u + 10000u), "wrap: due after the period across the wrap");
  }
  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "rtp_keepalive tests passed\n";
  return 0;
}
