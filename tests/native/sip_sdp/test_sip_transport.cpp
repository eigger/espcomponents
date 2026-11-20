#include "sip_transport.h"

#include <iostream>
#include <string>

using esphome::sip_client::sip_contact_uri;
using esphome::sip_client::sip_via_header;

namespace {

int g_failures = 0;

void require_eq_str(const std::string &actual, const std::string &expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got \"" << actual << "\", expected \"" << expected
              << "\")\n";
    g_failures++;
  }
}

}  // namespace

int main() {
  // These strings are what SipClient emitted before the transport
  // abstraction (hardcoded "SIP/2.0/UDP" + local ip:port); UDP wire output
  // must stay byte-identical.
  require_eq_str(sip_via_header("UDP", "192.168.1.50:5060", "z9hG4bK-abc"),
                 "Via: SIP/2.0/UDP 192.168.1.50:5060;branch=z9hG4bK-abc;rport\r\n",
                 "UDP Via header unchanged");
  require_eq_str(sip_contact_uri("16", "192.168.1.50:5060", ""), "<sip:16@192.168.1.50:5060>",
                 "UDP Contact unchanged");
  // Shape PR2 relies on for WebSocket.
  require_eq_str(sip_via_header("WSS", "abc123.invalid", "z9hG4bK-x"),
                 "Via: SIP/2.0/WSS abc123.invalid;branch=z9hG4bK-x;rport\r\n", "WSS Via header");
  require_eq_str(sip_contact_uri("16", "abc123.invalid", ";transport=ws"),
                 "<sip:16@abc123.invalid;transport=ws>", "WS Contact");
  if (g_failures != 0) return 1;
  std::cout << "sip_transport tests passed\n";
  return 0;
}
