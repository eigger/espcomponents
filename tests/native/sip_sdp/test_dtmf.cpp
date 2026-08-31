#include "dtmf.h"

#include <cstdlib>
#include <iostream>
#include <string>

using esphome::sip_client::DtmfRxDedup;
using esphome::sip_client::parse_dtmf_info;

namespace {

int g_failures = 0;

void require_eq_char(char actual, char expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (got '" << (actual == 0 ? '0' : actual)
              << "', expected '" << (expected == 0 ? '0' : expected) << "')\n";
    g_failures++;
  }
}

// ---------------- RFC 4733 receive de-duplication ----------------

void test_digit_fires_once_per_event() {
  // One digit = several packets sharing an RTP timestamp: the start packet,
  // duration updates, then the end packet retransmitted three times.
  DtmfRxDedup d;
  require_eq_char(d.feed(1, 1000), '1', "first packet of the event reports the digit");
  require_eq_char(d.feed(1, 1000), 0, "duration update must not repeat the digit");
  require_eq_char(d.feed(1, 1000), 0, "end packet must not repeat the digit");
  require_eq_char(d.feed(1, 1000), 0, "end retransmit must not repeat the digit");
}

void test_marker_less_sender_still_fires() {
  // Yealink DECT / 3CX Android send the event without the RTP marker bit. The
  // decoder never looks at the marker, so the first packet is enough.
  DtmfRxDedup d;
  require_eq_char(d.feed(5, 4242), '5', "marker-less first packet reports the digit");
}

void test_same_digit_pressed_twice() {
  // A second press carries a new RTP timestamp.
  DtmfRxDedup d;
  require_eq_char(d.feed(1, 1000), '1', "first press");
  require_eq_char(d.feed(1, 1000), 0, "repeat packet of the first press");
  require_eq_char(d.feed(1, 2600), '1', "second press of the same digit");
}

void test_star_hash_and_letters() {
  DtmfRxDedup d;
  require_eq_char(d.feed(10, 1), '*', "event 10 -> *");
  require_eq_char(d.feed(11, 2), '#', "event 11 -> #");
  require_eq_char(d.feed(12, 3), 'A', "event 12 -> A");
  require_eq_char(d.feed(15, 4), 'D', "event 15 -> D");
  require_eq_char(d.feed(16, 5), 0, "event 16 (flash) is dropped");
}

void test_reset_between_calls() {
  // stop()/start() clears the state, so an identical packet in the next call
  // (same event, same timestamp) is not swallowed as a duplicate.
  DtmfRxDedup d;
  require_eq_char(d.feed(7, 900), '7', "first call");
  d.reset();
  require_eq_char(d.feed(7, 900), '7', "next call reports the digit again");
}

// ---------------- SIP INFO ----------------

void test_info_dtmf_relay() {
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Signal=1\r\nDuration=160\r\n"), '1',
                  "Signal= digit");
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "signal = 4\r\nDuration=250"), '4',
                  "lowercase name and spaces around '='");
  require_eq_char(parse_dtmf_info("application/dtmf-relay;charset=utf-8", "Signal=9\r\n"), '9',
                  "Content-Type parameters are ignored");
}

void test_info_star_hash_spellings() {
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Signal=*\r\nDuration=160"), '*',
                  "literal *");
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Signal=10\r\nDuration=160"), '*',
                  "event code 10 -> *");
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Signal=11\r\nDuration=160"), '#',
                  "event code 11 -> #");
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Signal=b\r\n"), 'B',
                  "lowercase letter digit is normalised to upper case");
}

void test_info_plain_dtmf() {
  require_eq_char(parse_dtmf_info("application/dtmf", "1"), '1', "bare digit body");
  require_eq_char(parse_dtmf_info("application/dtmf", "#\r\n"), '#', "bare # with trailing CRLF");
}

void test_info_duration_is_not_a_digit() {
  // The naive "first digit-ish character in the body" approach reads 'D' out of
  // "Duration" and fires DTMF 'D'.
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Duration=250\r\n"), 0,
                  "Duration alone is not a signal");
}

void test_info_non_dtmf_is_ignored() {
  require_eq_char(parse_dtmf_info("application/media_control+xml", "<vc_primitive/>"), 0,
                  "video fast-update INFO must not fire DTMF");
  require_eq_char(parse_dtmf_info("", "Signal=1"), 0, "no Content-Type -> no DTMF");
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Signal=99\r\n"), 0,
                  "out-of-range event code");
  require_eq_char(parse_dtmf_info("application/dtmf-relay", "Signal=\r\n"), 0, "empty signal");
}

}  // namespace

int main() {
  test_digit_fires_once_per_event();
  test_marker_less_sender_still_fires();
  test_same_digit_pressed_twice();
  test_star_hash_and_letters();
  test_reset_between_calls();
  test_info_dtmf_relay();
  test_info_star_hash_spellings();
  test_info_plain_dtmf();
  test_info_duration_is_not_a_digit();
  test_info_non_dtmf_is_ignored();

  if (g_failures != 0) {
    std::cerr << g_failures << " failure(s)\n";
    return 1;
  }
  std::cout << "All sip_dtmf tests passed\n";
  return 0;
}
