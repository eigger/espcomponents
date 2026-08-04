#include "sdp_builder.h"

#include <cstdlib>
#include <iostream>
#include <string>

using esphome::sip_client::SdpMediaParams;
using esphome::sip_client::build_sdp_body;

namespace {

int g_failures = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    g_failures++;
  }
}

void require_contains(const std::string &hay, const std::string &needle, const char *message) {
  if (hay.find(needle) == std::string::npos) {
    std::cerr << "FAIL: " << message << " (missing \"" << needle << "\")\n";
    g_failures++;
  }
}

void require_not_contains(const std::string &hay, const std::string &needle, const char *message) {
  if (hay.find(needle) != std::string::npos) {
    std::cerr << "FAIL: " << message << " (unexpected \"" << needle << "\")\n";
    g_failures++;
  }
}

void test_offer_lists_both_g711_and_dtmf() {
  SdpMediaParams p;
  p.session_id = "1";
  p.local_ip = "192.168.0.2";
  p.rtp_port = 7078;
  p.direction = "sendrecv";
  p.answer = false;
  std::string sdp = build_sdp_body(p);
  require_contains(sdp, "m=audio 7078 RTP/AVP 0 8 101\r\n", "offer m-line");
  require_contains(sdp, "a=rtpmap:0 PCMU/8000\r\n", "offer pcmu");
  require_contains(sdp, "a=rtpmap:8 PCMA/8000\r\n", "offer pcma");
  require_contains(sdp, "a=rtpmap:101 telephone-event/8000\r\n", "offer dtmf");
  require_contains(sdp, "a=sendrecv\r\n", "offer direction");
}

void test_answer_single_codec_dynamic_pcma() {
  // Dynamic PT 97 mapped to PCMA — rtpmap label must say PCMA, not PCMU.
  SdpMediaParams p;
  p.session_id = "2";
  p.local_ip = "10.0.0.1";
  p.rtp_port = 4000;
  p.direction = "recvonly";
  p.answer = true;
  p.answer_pt = 97;
  p.answer_rtpmap = "PCMA/8000";
  p.dtmf_pt = 101;
  std::string sdp = build_sdp_body(p);
  require_contains(sdp, "m=audio 4000 RTP/AVP 97 101\r\n", "answer m-line");
  require_contains(sdp, "a=rtpmap:97 PCMA/8000\r\n", "answer dynamic pcma label");
  require_not_contains(sdp, "PCMU/8000", "answer must not list PCMU");
  require_not_contains(sdp, "RTP/AVP 0 8", "answer must not re-offer full list");
  require_contains(sdp, "a=rtpmap:101 telephone-event/8000\r\n", "answer dtmf");
  require_contains(sdp, "a=recvonly\r\n", "answer direction");
}

void test_answer_without_dtmf() {
  SdpMediaParams p;
  p.session_id = "3";
  p.local_ip = "10.0.0.2";
  p.rtp_port = 5000;
  p.direction = "sendonly";
  p.answer = true;
  p.answer_pt = 0;
  p.answer_rtpmap = "PCMU/8000";
  p.dtmf_pt = -1;
  std::string sdp = build_sdp_body(p);
  require_contains(sdp, "m=audio 5000 RTP/AVP 0\r\n", "answer no-dtmf m-line");
  require_not_contains(sdp, "telephone-event", "answer no-dtmf: no event");
}

}  // namespace

int main() {
  test_offer_lists_both_g711_and_dtmf();
  test_answer_single_codec_dynamic_pcma();
  test_answer_without_dtmf();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All sdp_builder tests passed\n";
  return 0;
}
