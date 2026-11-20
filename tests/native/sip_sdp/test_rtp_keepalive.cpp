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
    for (uint32_t t = 1000; t < 3000; t += 20) all = all && k.idle_frame_due(t);
    require_true(all, "period 0 sends every slot");
  }
  // thinned: burst, then one frame per period.
  {
    RtpKeepalive k;
    k.configure(10000);
    k.reset(0);
    require_true(k.thinned(), "thinned when period set");
    uint32_t t = 60;
    for (int i = 0; i < RtpKeepalive::BURST_FRAMES; i++, t += 20) {
      require_true(k.idle_frame_due(t), "burst frame due");
      k.on_sent(t);
    }
    require_true(!k.idle_frame_due(t), "no frame right after the burst");
    require_true(!k.idle_frame_due(t + 9000), "no frame before the period elapsed");
    uint32_t last = t - 20;
    require_true(k.idle_frame_due(last + 10000), "frame due once the period elapsed");
    k.on_sent(last + 10000);
    require_true(!k.idle_frame_due(last + 10020), "period restarts after a send");
    require_true(k.idle_frame_due(last + 20000), "second period frame");
  }
  // real audio re-arms the burst and defers the next keepalive.
  {
    RtpKeepalive k;
    k.configure(5000);
    k.reset(0);
    for (uint32_t t = 0; t < 100; t += 20) k.idle_frame_due(t);  // drain burst
    require_true(!k.idle_frame_due(200), "drained");
    k.on_audio(3000);
    require_true(k.idle_frame_due(3100), "burst again after audio");
    for (int i = 0; i < RtpKeepalive::BURST_FRAMES - 1; i++) k.idle_frame_due(3120);
    require_true(!k.idle_frame_due(3200), "burst finite after audio");
    require_true(!k.idle_frame_due(7900), "period counts from last audio");
    require_true(k.idle_frame_due(8000), "period elapsed after audio");
  }
  // millis() wrap.
  {
    RtpKeepalive k;
    k.configure(1000);
    k.reset(0xFFFFFF00u);
    for (int i = 0; i < RtpKeepalive::BURST_FRAMES; i++) k.idle_frame_due(0xFFFFFF00u);
    k.on_sent(0xFFFFFF00u);
    require_true(!k.idle_frame_due(0x100u), "wrap: not yet due");
    require_true(k.idle_frame_due(0x2F0u), "wrap: due after the period across the wrap");
  }
  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "rtp_keepalive tests passed\n";
  return 0;
}
