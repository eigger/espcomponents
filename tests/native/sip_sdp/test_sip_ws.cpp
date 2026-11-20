#include "sip_ws_util.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <string>

using esphome::sip_client::sip_random_invalid_host;
using esphome::sip_client::sip_ws_uri;
using esphome::sip_client::WsReassembler;

namespace {

int g_failures = 0;

void require_eq_str(const std::string &actual, const std::string &expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got \"" << actual << "\", expected \"" << expected << "\")\n";
    g_failures++;
  }
}

void require_true(bool cond, const char *message) {
  if (!cond) {
    std::cerr << "FAIL: " << message << "\n";
    g_failures++;
  }
}

// Feed a whole payload as buffer-sized chunks of one frame, like esp_websocket_client does.
WsReassembler::Result feed_frame(WsReassembler &r, uint8_t op, const std::string &payload, bool fin, size_t chunk,
                                 std::string &out) {
  WsReassembler::Result last = WsReassembler::NONE;
  size_t off = 0;
  do {
    size_t n = std::min(chunk, payload.size() - off);
    auto res = r.feed(op, payload.data() + off, static_cast<int>(n), static_cast<int>(payload.size()),
                      static_cast<int>(off), fin, out);
    if (res != WsReassembler::NONE) last = res;
    off += n;
  } while (off < payload.size());
  return last;
}

}  // namespace

int main() {
  // ---- .invalid host generator ----
  {
    uint32_t counter = 0;
    std::string h = sip_random_invalid_host([&]() { return counter++ * 7919u; });
    require_true(h.size() == 10 + 8, "host is 10 chars + .invalid");
    require_eq_str(h.substr(10), ".invalid", "host suffix");
    bool ok = true;
    for (size_t i = 0; i < 10; i++) ok = ok && (std::islower(static_cast<unsigned char>(h[i])) || std::isdigit(static_cast<unsigned char>(h[i])));
    require_true(ok, "host token is lowercase alphanumeric");
    // Extreme random values must stay inside the alphabet.
    std::string h2 = sip_random_invalid_host([]() { return 0xFFFFFFFFu; });
    require_eq_str(h2.substr(10), ".invalid", "max value suffix");
    require_true(sip_random_invalid_host([]() { return 0u; }) == "aaaaaaaaaa.invalid", "zero maps to first letter");
  }

  // ---- URI builder ----
  require_eq_str(sip_ws_uri(true, "pbx.example.com", 8089, "/ws"), "wss://pbx.example.com:8089/ws", "wss uri");
  require_eq_str(sip_ws_uri(false, "192.168.1.5", 8088, "/ws"), "ws://192.168.1.5:8088/ws", "ws uri");
  require_eq_str(sip_ws_uri(true, "h", 443, "sip"), "wss://h:443/sip", "path without slash gets one");
  require_eq_str(sip_ws_uri(true, "h", 443, ""), "wss://h:443/", "empty path");
  require_eq_str(sip_ws_uri(true, "fe80::1", 8089, "/ws"), "wss://[fe80::1]:8089/ws", "ipv6 literal bracketed");

  // ---- reassembly ----
  {
    WsReassembler r(64);
    std::string out;
    // Single small text frame.
    require_true(r.feed(WsReassembler::OP_TEXT, "INVITE", 6, 6, 0, true, out) == WsReassembler::MESSAGE, "single frame");
    require_eq_str(out, "INVITE", "single frame payload");

    // One frame split by the client's buffer size.
    std::string big(50, 'a');
    require_true(feed_frame(r, WsReassembler::OP_TEXT, big, true, 16, out) == WsReassembler::MESSAGE, "chunked frame");
    require_eq_str(out, big, "chunked frame payload");

    // Binary frames are accepted too.
    require_true(r.feed(WsReassembler::OP_BINARY, "BYE", 3, 3, 0, true, out) == WsReassembler::MESSAGE, "binary");
    require_eq_str(out, "BYE", "binary payload");

    // Message split over TEXT + CONTINUATION frames (FIN only on the last).
    require_true(r.feed(WsReassembler::OP_TEXT, "REGI", 4, 4, 0, false, out) == WsReassembler::NONE, "first fragment");
    require_true(r.feed(WsReassembler::OP_CONT, "STER", 4, 4, 0, true, out) == WsReassembler::MESSAGE, "last fragment");
    require_eq_str(out, "REGISTER", "fragmented payload");

    // Control frames are ignored, even in the middle of a fragmented message.
    require_true(r.feed(WsReassembler::OP_TEXT, "AB", 2, 2, 0, false, out) == WsReassembler::NONE, "frag 1");
    require_true(r.feed(0x9, "p", 1, 1, 0, true, out) == WsReassembler::NONE, "ping ignored");
    require_true(r.feed(0xA, "p", 1, 1, 0, true, out) == WsReassembler::NONE, "pong ignored");
    require_true(r.feed(0x8, "", 0, 0, 0, true, out) == WsReassembler::NONE, "close ignored");
    require_true(r.feed(WsReassembler::OP_CONT, "CD", 2, 2, 0, true, out) == WsReassembler::MESSAGE, "frag 2");
    require_eq_str(out, "ABCD", "control frame did not corrupt message");

    // Oversized: dropped once, whole remainder swallowed, next message fine.
    std::string huge(200, 'x');
    int oversized = 0, messages = 0;
    size_t off = 0;
    while (off < huge.size()) {
      size_t n = std::min<size_t>(32, huge.size() - off);
      auto res = r.feed(WsReassembler::OP_TEXT, huge.data() + off, static_cast<int>(n), static_cast<int>(huge.size()),
                        static_cast<int>(off), true, out);
      if (res == WsReassembler::OVERSIZED) oversized++;
      if (res == WsReassembler::MESSAGE) messages++;
      off += n;
    }
    require_true(oversized == 1 && messages == 0, "oversized message reported once, never delivered");
    require_true(r.feed(WsReassembler::OP_TEXT, "OK", 2, 2, 0, true, out) == WsReassembler::MESSAGE, "recovers after oversized");
    require_eq_str(out, "OK", "payload after oversized");

    // Oversized by accumulating fragments.
    r.reset();
    std::string half(40, 'y');
    require_true(r.feed(WsReassembler::OP_TEXT, half.data(), 40, 40, 0, false, out) == WsReassembler::NONE, "half 1");
    require_true(r.feed(WsReassembler::OP_CONT, half.data(), 40, 40, 0, true, out) == WsReassembler::OVERSIZED, "half 2 overflows");
    require_true(r.feed(WsReassembler::OP_TEXT, "Z", 1, 1, 0, true, out) == WsReassembler::MESSAGE, "recovers after fragment overflow");

    // Stray continuation is ignored, next message ok.
    r.reset();
    require_true(r.feed(WsReassembler::OP_CONT, "junk", 4, 4, 0, true, out) == WsReassembler::NONE, "stray continuation");
    require_true(r.feed(WsReassembler::OP_TEXT, "ok", 2, 2, 0, true, out) == WsReassembler::MESSAGE, "after stray");

    // reset() discards a half-built message (connection dropped).
    require_true(r.feed(WsReassembler::OP_TEXT, "half", 4, 8, 0, true, out) == WsReassembler::NONE, "partial chunk");
    r.reset();
    require_true(r.feed(WsReassembler::OP_TEXT, "new", 3, 3, 0, true, out) == WsReassembler::MESSAGE, "fresh after reset");
    require_eq_str(out, "new", "no leftovers after reset");

    // Empty payloads produce nothing.
    require_true(r.feed(WsReassembler::OP_TEXT, "", 0, 0, 0, true, out) == WsReassembler::NONE, "empty message");
  }

  if (g_failures != 0) return 1;
  std::cout << "sip_ws tests passed\n";
  return 0;
}
