#include "sip_message.h"

#include <cstdlib>
#include <iostream>
#include <string>

using esphome::sip_client::SdpInfo;
using esphome::sip_client::parse_sdp;

namespace {

int g_failures = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    g_failures++;
  }
}

void require_eq(int actual, int expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got " << actual << ", expected " << expected << ")\n";
    g_failures++;
  }
}

void require_eq_str(const std::string &actual, const std::string &expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got \"" << actual << "\", expected \"" << expected
              << "\")\n";
    g_failures++;
  }
}

void test_dynamic_pt_before_g722_no_false_positive() {
  // Real PBX offers often list dynamic PTs (96..) before static 9 / 0 / 8.
  // Substring search for " 9" would wrongly match 96..99.
  const char *body =
      "v=0\r\n"
      "o=- 0 0 IN IP4 192.168.0.10\r\n"
      "s=-\r\n"
      "c=IN IP4 192.168.0.10\r\n"
      "t=0 0\r\n"
      "m=audio 12345 RTP/AVP 96 9 0 8 101\r\n"
      "a=rtpmap:96 opus/48000/2\r\n"
      "a=rtpmap:9 G722/8000\r\n"
      "a=rtpmap:0 PCMU/8000\r\n"
      "a=rtpmap:8 PCMA/8000\r\n"
      "a=rtpmap:101 telephone-event/8000\r\n";

  SdpInfo sdp = parse_sdp(body);
  require(sdp.valid, "dynamic-before-static: valid");
  require_eq(sdp.audio_port, 12345, "dynamic-before-static: port");
  require_eq_str(sdp.connection_ip, "192.168.0.10", "dynamic-before-static: c=");
  require_eq((int) sdp.payload_types.size(), 5, "dynamic-before-static: fmt count");
  require(sdp.has_payload_type(96), "has 96");
  require(sdp.has_payload_type(9), "has 9");
  require(sdp.has_payload_type(0), "has 0");
  require(sdp.has_payload_type(8), "has 8");
  require(sdp.has_payload_type(101), "has 101");
  require(!sdp.has_payload_type(97), "does not invent 97 from substring");
  require_eq(sdp.pcmu_pt, 0, "pcmu from rtpmap/static");
  require_eq(sdp.pcma_pt, 8, "pcma from rtpmap/static");
  require_eq(sdp.telephone_event_pt, 101, "telephone-event");
  // G.722 is present in the list but not yet a convenience field — still in rtpmap.
  require(sdp.rtpmap.count(9) == 1, "g722 rtpmap retained");
  require_eq_str(sdp.rtpmap[9], "g722/8000", "g722 rtpmap value");
}

void test_static_only_without_rtpmap() {
  const char *body =
      "v=0\r\n"
      "c=IN IP4 10.0.0.1\r\n"
      "m=audio 7078 RTP/AVP 0 8\r\n";

  SdpInfo sdp = parse_sdp(body);
  require(sdp.valid, "static-only: valid");
  require_eq(sdp.pcmu_pt, 0, "static-only: pcmu via PT 0");
  require_eq(sdp.pcma_pt, 8, "static-only: pcma via PT 8");
  require_eq(sdp.telephone_event_pt, -1, "static-only: no telephone-event");
}

void test_no_telephone_event() {
  const char *body =
      "c=IN IP4 10.0.0.2\r\n"
      "m=audio 4000 RTP/AVP 8 0\r\n"
      "a=rtpmap:8 PCMA/8000\r\n"
      "a=rtpmap:0 PCMU/8000\r\n";

  SdpInfo sdp = parse_sdp(body);
  require_eq(sdp.telephone_event_pt, -1, "no dtmf: telephone_event_pt");
  require_eq(sdp.pcma_pt, 8, "no dtmf: pcma preferred listing still found");
  require_eq(sdp.pcmu_pt, 0, "no dtmf: pcmu");
}

void test_no_common_g711() {
  const char *body =
      "c=IN IP4 10.0.0.3\r\n"
      "m=audio 5000 RTP/AVP 96 97\r\n"
      "a=rtpmap:96 opus/48000/2\r\n"
      "a=rtpmap:97 telephone-event/8000\r\n";

  SdpInfo sdp = parse_sdp(body);
  require(sdp.valid, "no-g711: valid m-line");
  require_eq(sdp.pcmu_pt, -1, "no-g711: no pcmu");
  require_eq(sdp.pcma_pt, -1, "no-g711: no pcma");
  require_eq(sdp.telephone_event_pt, 97, "no-g711: dtmf still parsed");
}

void test_pt96_through_99_not_confused_with_9() {
  // Even without an explicit G722 rtpmap, " 9" must not be invented from 96-99.
  const char *body =
      "m=audio 8000 RTP/AVP 96 97 98 99 0\r\n"
      "a=rtpmap:96 foo/8000\r\n"
      "a=rtpmap:97 bar/8000\r\n"
      "a=rtpmap:98 baz/8000\r\n"
      "a=rtpmap:99 qux/8000\r\n"
      "a=rtpmap:0 PCMU/8000\r\n";

  SdpInfo sdp = parse_sdp(body);
  require(!sdp.has_payload_type(9), "96-99 must not imply PT 9");
  require_eq((int) sdp.payload_types.size(), 5, "exact five fmts");
  require_eq(sdp.pcmu_pt, 0, "pcmu still found");
}

void test_audio_plus_video_ignores_video_section() {
  const char *body =
      "c=IN IP4 192.168.1.1\r\n"
      "m=audio 7078 RTP/AVP 0 8 101\r\n"
      "a=rtpmap:0 PCMU/8000\r\n"
      "a=rtpmap:8 PCMA/8000\r\n"
      "a=rtpmap:101 telephone-event/8000\r\n"
      "m=video 9078 RTP/AVP 96 97\r\n"
      "c=IN IP4 10.0.0.99\r\n"
      "a=rtpmap:96 H264/90000\r\n"
      "a=rtpmap:97 telephone-event/8000\r\n";

  SdpInfo sdp = parse_sdp(body);
  require(sdp.valid, "av: audio valid");
  require_eq(sdp.audio_port, 7078, "av: audio port");
  require_eq_str(sdp.connection_ip, "192.168.1.1", "av: video c= must not override");
  require_eq((int) sdp.payload_types.size(), 3, "av: only audio fmts");
  require(!sdp.has_payload_type(96), "av: video PT 96 excluded");
  require_eq(sdp.telephone_event_pt, 101, "av: audio dtmf, not video 97");
  require(sdp.rtpmap.count(96) == 0, "av: video rtpmap excluded");
}

void test_non_numeric_fmt_token_skipped() {
  const char *body =
      "m=audio 7078 RTP/AVP 0 * 8\r\n"
      "a=rtpmap:0 PCMU/8000\r\n"
      "a=rtpmap:8 PCMA/8000\r\n";

  SdpInfo sdp = parse_sdp(body);
  require_eq((int) sdp.payload_types.size(), 2, "non-numeric: only 0 and 8");
  require(sdp.has_payload_type(0), "non-numeric: has 0");
  require(sdp.has_payload_type(8), "non-numeric: has 8");
  require(!sdp.has_payload_type(42), "non-numeric: no invented PT");
}

void test_rejected_audio_stream_port_zero() {
  const char *body =
      "c=IN IP4 192.168.0.5\r\n"
      "m=audio 0 RTP/AVP 0\r\n"
      "a=rtpmap:0 PCMU/8000\r\n";

  SdpInfo sdp = parse_sdp(body);
  require(sdp.valid, "port0: still a valid audio description");
  require_eq(sdp.audio_port, 0, "port0: port is zero");
  require_eq(sdp.pcmu_pt, 0, "port0: pcmu still derived");
}

}  // namespace

int main() {
  test_dynamic_pt_before_g722_no_false_positive();
  test_static_only_without_rtpmap();
  test_no_telephone_event();
  test_no_common_g711();
  test_pt96_through_99_not_confused_with_9();
  test_audio_plus_video_ignores_video_section();
  test_non_numeric_fmt_token_skipped();
  test_rejected_audio_stream_port_zero();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All sip_sdp tests passed\n";
  return 0;
}
