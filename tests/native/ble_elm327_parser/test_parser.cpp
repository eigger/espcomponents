#include "response_parser.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using esphome::ble_elm327::normalize_command;
using esphome::ble_elm327::parse_response_bytes;

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

  if (g_failures > 0) {
    std::cerr << g_failures << " ble_elm327 parser test(s) failed\n";
    return EXIT_FAILURE;
  }

  std::cout << "All ble_elm327 parser tests passed.\n";
  return EXIT_SUCCESS;
}
