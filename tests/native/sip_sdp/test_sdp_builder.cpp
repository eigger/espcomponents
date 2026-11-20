#include "sdp_builder.h"
#include "codec.h"

#include <cstdlib>
#include <iostream>
#include <string>

using esphome::sip_client::AudioCodecId;
using esphome::sip_client::SdpMediaParams;
using esphome::sip_client::build_sdp_body;

namespace {

int g_failures = 0;

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

void test_offer_g722_preferred() {
  SdpMediaParams p;
  p.session_id = "4";
  p.local_ip = "192.168.0.3";
  p.rtp_port = 7078;
  p.direction = "sendrecv";
  p.answer = false;
  p.offer_codecs = {AudioCodecId::G722, AudioCodecId::PCMU};
  std::string sdp = build_sdp_body(p);
  require_contains(sdp, "m=audio 7078 RTP/AVP 9 0 101\r\n", "g722 offer m-line order");
  require_contains(sdp, "a=rtpmap:9 G722/8000\r\n", "g722 offer rtpmap");
  require_contains(sdp, "a=rtpmap:0 PCMU/8000\r\n", "g722 offer pcmu");
  require_not_contains(sdp, "PCMA/8000", "g722 offer excludes pcma when not listed");
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

void test_origin_version_defaults_to_session_id() {
  SdpMediaParams p;
  p.session_id = "77";
  p.local_ip = "10.0.0.5";
  std::string sdp = build_sdp_body(p);
  require_contains(sdp, "o=- 77 77 IN IP4 10.0.0.5\r\n", "no explicit version: same as session id");
}

void test_origin_version_explicit() {
  SdpMediaParams p;
  p.session_id = "77";
  p.session_version = "79";
  p.local_ip = "10.0.0.5";
  std::string sdp = build_sdp_body(p);
  require_contains(sdp, "o=- 77 79 IN IP4 10.0.0.5\r\n", "session id stays, version differs");

  // Same params -> byte-identical body (a stable version means a stable body).
  require_contains(build_sdp_body(p), sdp, "deterministic body");
  p.session_version = "80";
  std::string next = build_sdp_body(p);
  require_contains(next, "o=- 77 80 IN IP4 10.0.0.5\r\n", "version bumped, id unchanged");
}

void test_negotiated_direction_in_body() {
  const char *dirs[] = {"sendrecv", "sendonly", "recvonly", "inactive"};
  for (const char *d : dirs) {
    SdpMediaParams p;
    p.session_id = "1";
    p.local_ip = "10.0.0.5";
    p.answer = true;
    p.answer_pt = 8;
    p.answer_rtpmap = "PCMA/8000";
    p.dtmf_pt = 101;
    p.direction = d;
    std::string sdp = build_sdp_body(p);
    require_contains(sdp, std::string("a=") + d + "\r\n", d);
  }
  SdpMediaParams hold;
  hold.session_id = "1";
  hold.local_ip = "10.0.0.5";
  hold.answer = true;
  hold.direction = "inactive";
  std::string sdp = build_sdp_body(hold);
  require_not_contains(sdp, "a=sendrecv", "hold answer has no sendrecv");
  require_contains(sdp, "m=audio 7078 RTP/AVP 0\r\n", "hold answer keeps the media line (port stays open)");
}

}  // namespace

int main() {
  test_offer_lists_both_g711_and_dtmf();
  test_offer_g722_preferred();
  test_answer_single_codec_dynamic_pcma();
  test_answer_without_dtmf();
  test_origin_version_defaults_to_session_id();
  test_origin_version_explicit();
  test_negotiated_direction_in_body();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All sdp_builder tests passed\n";
  return 0;
}
