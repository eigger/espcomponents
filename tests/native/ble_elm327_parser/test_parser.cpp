#include "response_parser.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using esphome::ble_elm327::normalize_command;
using esphome::ble_elm327::parse_response_bytes;
using esphome::ble_elm327::classify_error_response;
using esphome::ble_elm327::counts_as_link_failure;
using esphome::ble_elm327::is_bus_error;
using esphome::ble_elm327::is_protocol_search_in_progress;
using esphome::ble_elm327::reconnect_backoff_ms;
using esphome::ble_elm327::check_response_timeout;
using esphome::ble_elm327::pre_command_reply_invalidates_header;
using esphome::ble_elm327::pre_commands_applied;
using esphome::ble_elm327::command_resets_adapter;
using esphome::ble_elm327::decide_dequeue;
using esphome::ble_elm327::DequeueAction;

namespace {

int g_failures = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    g_failures++;
  }
}

void require_eq(const std::vector<uint8_t> &actual, const std::vector<uint8_t> &expected, const char *message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " (size " << actual.size() << " vs " << expected.size() << ")\n";
    std::cerr << "  actual:  ";
    for (auto b : actual) std::cerr << std::hex << (int) b << ' ';
    std::cerr << "\n  expected:";
    for (auto b : expected) std::cerr << std::hex << (int) b << ' ';
    std::cerr << std::dec << '\n';
    g_failures++;
  }
}

// The bug fixed by PR #294: ELM327 adapters prefix multiline responses with
// hexadecimal sequence indices once a response exceeds 10 lines (0-9, then
// A-F). Validating the index with std::isdigit (decimal only) instead of
// std::isxdigit truncated any response spanning 11+ lines right after "9:".
void test_multiline_hex_index_regression_uppercase() {
  // Two data lines: "9: AA BB" then "A: CC DD" — a naive isdigit check would
  // reject the "A:" line, so the assembled payload would stop at AABB.
  std::string response = "9: AA BB\r\nA: CC DD\r\n";
  auto bytes = parse_response_bytes(response, "");
  require_eq(bytes, {0xAA, 0xBB, 0xCC, 0xDD},
             "hex index regression: line 'A:' must not truncate the payload");
}

void test_multiline_hex_index_regression_full_range() {
  // Simulates the PR's real-world report: a response spanning all 16
  // possible sequence indices (0-9, A-F), each carrying 2 bytes.
  std::ostringstream response;
  std::vector<uint8_t> expected;
  for (int i = 0; i < 16; i++) {
    char index = i < 10 ? static_cast<char>('0' + i) : static_cast<char>('A' + (i - 10));
    uint8_t b0 = static_cast<uint8_t>(i * 2);
    uint8_t b1 = static_cast<uint8_t>(i * 2 + 1);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02X %02X", b0, b1);
    response << index << ": " << buf << "\r\n";
    expected.push_back(b0);
    expected.push_back(b1);
  }
  auto bytes = parse_response_bytes(response.str(), "");
  require_eq(bytes, expected, "hex index regression: full 0-F sequence assembles without truncation");
}

void test_multiline_hex_index_lowercase() {
  std::string response = "9: 01 02\r\nb: 03 04\r\n";
  auto bytes = parse_response_bytes(response, "");
  require_eq(bytes, {0x01, 0x02, 0x03, 0x04}, "hex index: lowercase index letters (b) are also accepted");
}

void test_multiline_numeric_indices_only() {
  std::string response = "0: 62 22 74\r\n1: CB 01 02\r\n";
  auto bytes = parse_response_bytes(response, "");
  require_eq(bytes, {0x62, 0x22, 0x74, 0xCB, 0x01, 0x02}, "multiline: plain numeric indices assemble in order");
}

void test_singleline_response_no_index() {
  std::string response = "41 0C 1A F8\r\n";
  auto bytes = parse_response_bytes(response, "");
  require_eq(bytes, {0x41, 0x0C, 0x1A, 0xF8}, "singleline: no colon-index prefix parses as one frame");
}

void test_command_echo_is_skipped() {
  // The adapter echoes the command back before the actual response line.
  std::string response = "010c\r\n41 0C 1A F8\r\n";
  auto bytes = parse_response_bytes(response, normalize_command("010C"));
  require_eq(bytes, {0x41, 0x0C, 0x1A, 0xF8}, "echo: echoed command line is excluded from the payload");
}

void test_non_hex_prefixed_colon_line_is_not_multiline() {
  // "OK" contains no hex digits, so this must fall back to the singleline
  // path (and, since it yields too little hex, produce no frame) rather
  // than being misidentified as a multiline index line.
  std::string response = "OK: 41\r\n";
  auto bytes = parse_response_bytes(response, "");
  require(bytes.empty(), "non-hex prefix: 'OK:' must not be treated as a multiline index");
}

void test_short_response_is_ignored() {
  // Fewer than 4 hex characters (2 bytes) is not a usable frame.
  std::string response = "41\r\n";
  auto bytes = parse_response_bytes(response, "");
  require(bytes.empty(), "short response: single byte of hex is ignored");
}

void test_odd_length_hex_drops_trailing_nibble() {
  std::string response = "41 0C 1\r\n";
  auto bytes = parse_response_bytes(response, "");
  require_eq(bytes, {0x41, 0x0C}, "odd length: trailing incomplete nibble is dropped, not rounded");
}

void test_blank_lines_and_whitespace_are_ignored() {
  std::string response = "\r\n   \r\n0: 41 0C\r\n\r\n1: 1A F8\r\n";
  auto bytes = parse_response_bytes(response, "");
  require_eq(bytes, {0x41, 0x0C, 0x1A, 0xF8}, "whitespace: blank/whitespace-only lines don't break assembly");
}

void test_empty_response_yields_no_bytes() {
  auto bytes = parse_response_bytes("", "");
  require(bytes.empty(), "empty response: yields no bytes");
}

void test_normalize_command_strips_whitespace_and_case() {
  require(normalize_command(" AT Z \r") == "atz", "normalize_command: trims and lowercases");
  require(normalize_command("010C") == "010c", "normalize_command: lowercases hex command");
}

void test_classify_error_responses() {
  require(classify_error_response("NO DATA\r\r", "010c") == "NO DATA", "classify: NO DATA");
  require(classify_error_response("STOPPED\r", "010c") == "STOPPED", "classify: STOPPED");
  require(classify_error_response("?\r", "atxx") == "?", "classify: unknown command '?'");
  require(classify_error_response("BUFFER FULL\r", "2201") == "BUFFER FULL", "classify: BUFFER FULL");
  require(classify_error_response("CAN ERROR\r", "010c") == "CAN ERROR", "classify: CAN ERROR");
  require(classify_error_response("SEARCHING...\rUNABLE TO CONNECT\r", "010c") == "UNABLE TO CONNECT",
          "classify: UNABLE TO CONNECT after SEARCHING");
  require(classify_error_response("ERR94\r", "010c") == "ERR94", "classify: ERRxx code");
  require(classify_error_response("no data", "010c") == "NO DATA", "classify: lowercase input");
}

void test_classify_ignores_normal_responses() {
  require(classify_error_response("410C1AF8\r", "010c").empty(), "classify: data line is not an error");
  require(classify_error_response("SEARCHING...\r410C1AF8\r", "010c").empty(), "classify: SEARCHING is not an error");
  require(classify_error_response("OK\r", "ate0").empty(), "classify: OK is not an error");
  require(classify_error_response("ELM327 v1.5\r", "atz").empty(), "classify: ATZ banner is not an error");
  require(classify_error_response("0: 62 E0 02\r1: AA BB\r", "22e002").empty(), "classify: multiline data");
  require(classify_error_response("", "010c").empty(), "classify: empty response");
  require(classify_error_response("010C\r410C1AF8\r", "010c").empty(), "classify: echo line is skipped");
}

void test_classify_bus_init_and_act_alert() {
  require(classify_error_response("BUS INIT: ...ERROR\r", "0100") == "BUS INIT ERROR", "classify: BUS INIT ERROR");
  require(classify_error_response("BUS INIT: ...OK\r4100BE3FA813\r", "0100").empty(), "classify: BUS INIT OK is not an error");
  require(classify_error_response("ACT ALERT\r", "010c") == "ACT ALERT", "classify: ACT ALERT");
  require(classify_error_response("LV RESET\r", "010c") == "LV RESET", "classify: LV RESET");
  require(classify_error_response("<RX ERROR\r", "010c") == "RX ERROR", "classify: RX ERROR");
  require(classify_error_response("DATA ERROR\r", "010c") == "DATA ERROR", "classify: DATA ERROR");
}

void test_error_categories() {
  // Vehicle-side conditions (normal with ignition off) do not count as link failures.
  const char *bus[] = {"NO DATA", "CAN ERROR", "BUS ERROR", "BUS INIT ERROR", "UNABLE TO CONNECT", "DATA ERROR"};
  for (const char *e : bus) {
    require(is_bus_error(e), "category: bus error recognized");
    require(!counts_as_link_failure(e, "010c"), "category: bus error is not a link failure");
  }
  const char *adapter[] = {"STOPPED", "BUFFER FULL", "LV RESET", "ACT ALERT", "ERR94"};
  for (const char *e : adapter) {
    require(!is_bus_error(e), "category: adapter error is not a bus error");
    require(counts_as_link_failure(e, "010c"), "category: adapter error is a link failure");
  }
  require(counts_as_link_failure("?", "010c"), "category: '?' to a PID request is a link failure");
  require(!counts_as_link_failure("?", "atsh7e4"), "category: '?' to an AT command is not a link failure");
  require(!counts_as_link_failure("", "010c"), "category: no error is not a failure");
}

void test_protocol_search_detection() {
  require(is_protocol_search_in_progress("SEARCHING..."), "search: SEARCHING");
  require(is_protocol_search_in_progress("searching..."), "search: lowercase");
  require(is_protocol_search_in_progress("BUS INIT: ..."), "search: BUS INIT");
  require(!is_protocol_search_in_progress("410C1A"), "search: data is not a search");
  require(!is_protocol_search_in_progress(""), "search: empty buffer");
}

void test_stopped_after_timeout_is_not_a_failure() {
  require(counts_as_link_failure("STOPPED", "010c", false), "STOPPED normally counts as a link failure");
  require(!counts_as_link_failure("STOPPED", "010c", true),
          "STOPPED right after a timeout (adapter aborted the old request) does not count");
  require(counts_as_link_failure("BUFFER FULL", "010c", true), "other adapter errors still count after a timeout");
}

void test_reconnect_backoff() {
  require(reconnect_backoff_ms(0) == 0, "backoff: none before the first reconnect");
  require(reconnect_backoff_ms(1) == 30000, "backoff: 30 s after one reconnect");
  require(reconnect_backoff_ms(2) == 60000, "backoff: doubles");
  require(reconnect_backoff_ms(5) == 480000, "backoff: 480 s after five");
  require(reconnect_backoff_ms(6) == 600000, "backoff: capped at 10 min");
  require(reconnect_backoff_ms(255) == 600000, "backoff: stays capped");
}

void test_default_timeout_matches_previous_behaviour() {
  // auto_recovery off: fixed timeout from the write; received data, SEARCHING
  // and the hard limit make no difference (same as before the option existed).
  auto r = check_response_timeout(false, 5000, false, 4999, 0);
  require(!r.expired, "default: not expired before 5000 ms after the write");
  r = check_response_timeout(false, 5000, false, 5001, 0);
  require(r.expired && !r.hard_limit_hit, "default: expired 5000 ms after the write even if data just arrived");
  r = check_response_timeout(false, 5000, true, 5001, 0);
  require(r.expired, "default: SEARCHING does not extend the timeout");
  require(r.timeout_ms == 5000 && r.hard_limit_ms == 5000, "default: reports the plain timeout");
}

void test_auto_recovery_timeout_rules() {
  auto r = check_response_timeout(true, 1500, false, 6000, 1000);
  require(!r.expired, "auto: activity keeps the exchange alive");
  r = check_response_timeout(true, 1500, false, 1600, 1600);
  require(r.expired && !r.hard_limit_hit, "auto: idle timeout expires");
  r = check_response_timeout(true, 1500, true, 15000, 15000);
  require(!r.expired && r.timeout_ms == 20000, "auto: SEARCHING raises the idle timeout to 20 s");
  r = check_response_timeout(true, 1500, true, 20001, 20001);
  require(r.expired, "auto: search timeout expires after 20 s");
  r = check_response_timeout(true, 1500, false, 30001, 100);
  require(r.expired && r.hard_limit_hit && r.hard_limit_ms == 30000, "auto: hard limit 30 s from the write");
  r = check_response_timeout(true, 20000, false, 39999, 100);
  require(!r.expired && r.hard_limit_ms == 40000, "auto: hard limit is 2x a long timeout");
}

void test_pre_command_header_invalidation() {
  require(!pre_command_reply_invalidates_header("?"),
          "pre_command: '?' (unsupported) keeps the header sticky, no re-send every poll");
  require(!pre_command_reply_invalidates_header(""), "pre_command: normal reply keeps the header");
  require(!pre_command_reply_invalidates_header("NO DATA"), "pre_command: bus errors keep the header");
  require(!pre_command_reply_invalidates_header("CAN ERROR"), "pre_command: bus errors keep the header");
  require(pre_command_reply_invalidates_header("STOPPED"), "pre_command: STOPPED forces a re-send");
  require(pre_command_reply_invalidates_header("BUFFER FULL"), "pre_command: adapter errors force a re-send");
  require(pre_command_reply_invalidates_header("ERR94"), "pre_command: ERRxx forces a re-send");
}

void test_pre_commands_applied() {
  const std::vector<std::string> tcm = {"ATSH 7E2\r"};
  const std::vector<std::string> none;
  require(pre_commands_applied(tcm, tcm), "header: normal path (pre_commands tracked when queued) sends the PID");
  require(pre_commands_applied(none, none), "header: device without pre_commands is sent");
  require(!pre_commands_applied(tcm, none), "header: cleared after a pre-command failure -> PID is held back");
  require(!pre_commands_applied(tcm, {"ATSH 7E0\r"}), "header: different header -> PID is held back");
}

void test_command_resets_adapter() {
  require(command_resets_adapter(normalize_command("ATZ\r")), "reset: ATZ");
  require(command_resets_adapter(normalize_command("AT WS")), "reset: ATWS");
  require(command_resets_adapter(normalize_command("at d")), "reset: ATD");
  require(!command_resets_adapter(normalize_command("ATDP")), "reset: ATDP (describe protocol) is not a reset");
  require(!command_resets_adapter(normalize_command("ATSH 7E0")), "reset: ATSH is not a reset");
  require(!command_resets_adapter(normalize_command("010C")), "reset: PID request is not a reset");
}

// Mirrors loop(): at collection the device's pre_commands become the tracked
// header and pre items + PID are queued; then each item is dequeued in order.
// Returns how many items were written and whether the PID was written.
struct CycleResult {
  int written;
  bool pid_sent;
};
// `pre_failure`: outcome of the device's pre items in this cycle. "" = all
// answered normally, "TIMEOUT" = timed out / write failed (loop() clears the
// tracked header in fail_exchange_), otherwise an error reply that is passed
// through pre_command_reply_invalidates_header() like handle_error_response_().
CycleResult run_poll_cycle(const std::vector<std::string> &pre, std::vector<std::string> &current,
                           const std::string &pre_failure = "") {
  struct Item {
    std::string cmd;
    bool pre;
    bool is_device;
  };
  std::vector<Item> queue;
  if (pre != current) {
    for (const auto &c : pre) queue.push_back({c, true, false});
    current = pre;
  }
  queue.push_back({"2219401\r", false, true});
  CycleResult r{0, false};
  for (const auto &item : queue) {
    auto action = decide_dequeue(item.pre, item.is_device ? &pre : nullptr, true, current, normalize_command(item.cmd));
    if (action == DequeueAction::HOLD_BACK) continue;
    if (action == DequeueAction::SEND_AND_CLEAR_HEADER) current.clear();
    r.written++;
    if (item.is_device) r.pid_sent = true;
    if (item.pre && !pre_failure.empty() &&
        (pre_failure == "TIMEOUT" || pre_command_reply_invalidates_header(pre_failure))) {
      current.clear();
    }
  }
  return r;
}

void test_failed_pre_command_cycle() {
  // ATSH fails -> tracked header cleared -> PID held back (not sent with the
  // wrong header) -> next poll re-sends ATSH + PID -> then PID only.
  const std::vector<std::string> pre = {"ATSH 7E4\r"};
  for (const char *failure : {"TIMEOUT", "STOPPED", "BUFFER FULL"}) {
    std::vector<std::string> current;
    auto failed = run_poll_cycle(pre, current, failure);
    require(!failed.pid_sent && failed.written == 1, "failed ATSH: only ATSH written, PID held back");
    require(current.empty(), "failed ATSH: tracked header cleared");
    auto retry = run_poll_cycle(pre, current);
    require(retry.pid_sent && retry.written == 2, "failed ATSH: next poll re-sends ATSH + PID");
    auto steady = run_poll_cycle(pre, current);
    require(steady.pid_sent && steady.written == 1, "failed ATSH: header sticky again, PID only");
  }
  // '?' (unsupported) keeps the header sticky: PID still sent, no re-send loop.
  std::vector<std::string> current;
  auto unsupported = run_poll_cycle(pre, current, "?");
  require(unsupported.pid_sent && unsupported.written == 2, "'?' to ATSH: PID sent, header kept");
  auto next = run_poll_cycle(pre, current);
  require(next.pid_sent && next.written == 1, "'?' to ATSH: no ATSH re-send on the next poll");
}

void test_reset_commands_inside_pre_commands_do_not_livelock() {
  // Regression: a reset command inside a device's own pre_commands used to
  // clear the tracked header and hold the PID back on every poll.
  const std::vector<std::vector<std::string>> configs = {
      {"ATD\r", "ATSP6\r", "ATSH 7E4\r"},
      {"ATZ\r", "ATSH 7E4\r"},
      {"AT WS\r", "ATSH 7E2\r"},
  };
  for (const auto &pre : configs) {
    std::vector<std::string> current;
    auto first = run_poll_cycle(pre, current);
    require(first.pid_sent, "reset in pre_commands: PID is sent on the first poll");
    require(first.written == static_cast<int>(pre.size()) + 1, "reset in pre_commands: pre list + PID written once");
    auto second = run_poll_cycle(pre, current);
    require(second.pid_sent && second.written == 1, "reset in pre_commands: header sticky, next poll sends only the PID");
  }
}

void test_decide_dequeue() {
  const std::vector<std::string> tcm = {"ATSH 7E2\r"};
  const std::vector<std::string> none;
  require(decide_dequeue(false, &tcm, true, tcm, "221940") == DequeueAction::SEND, "dequeue: normal PID is sent");
  require(decide_dequeue(false, &tcm, true, none, "221940") == DequeueAction::HOLD_BACK,
          "dequeue: PID whose header was lost is held back");
  require(decide_dequeue(false, &tcm, false, none, "221940") == DequeueAction::SEND,
          "dequeue: no header check outside READY");
  require(decide_dequeue(false, nullptr, true, tcm, "atz") == DequeueAction::SEND_AND_CLEAR_HEADER,
          "dequeue: standalone ATZ (send_command / init) clears the header");
  require(decide_dequeue(true, nullptr, true, tcm, "atz") == DequeueAction::SEND,
          "dequeue: ATZ as a device pre item keeps the header tracking");
  require(decide_dequeue(false, nullptr, true, tcm, "atsh7e0") == DequeueAction::SEND, "dequeue: other AT command");
}

}  // namespace

int main() {
  test_multiline_hex_index_regression_uppercase();
  test_multiline_hex_index_regression_full_range();
  test_multiline_hex_index_lowercase();
  test_multiline_numeric_indices_only();
  test_singleline_response_no_index();
  test_command_echo_is_skipped();
  test_non_hex_prefixed_colon_line_is_not_multiline();
  test_short_response_is_ignored();
  test_odd_length_hex_drops_trailing_nibble();
  test_blank_lines_and_whitespace_are_ignored();
  test_empty_response_yields_no_bytes();
  test_normalize_command_strips_whitespace_and_case();
  test_classify_error_responses();
  test_classify_ignores_normal_responses();
  test_classify_bus_init_and_act_alert();
  test_error_categories();
  test_protocol_search_detection();
  test_stopped_after_timeout_is_not_a_failure();
  test_reconnect_backoff();
  test_default_timeout_matches_previous_behaviour();
  test_auto_recovery_timeout_rules();
  test_pre_command_header_invalidation();
  test_pre_commands_applied();
  test_command_resets_adapter();
  test_reset_commands_inside_pre_commands_do_not_livelock();
  test_decide_dequeue();
  test_failed_pre_command_cycle();

  if (g_failures > 0) {
    std::cerr << g_failures << " ble_elm327 parser test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All ble_elm327 parser tests passed.\n";
  return EXIT_SUCCESS;
}
