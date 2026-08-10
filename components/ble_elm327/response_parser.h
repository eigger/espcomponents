#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Framework-independent ELM327 response parsing. Kept free of ESPHome/ESP-IDF
// headers so it can be compiled and unit-tested natively (see
// tests/native/ble_elm327_parser).

namespace esphome {
namespace ble_elm327 {

// Lowercases and strips whitespace (including internal whitespace) from a
// command, matching the normalization applied before commands are queued
// for transmission (see BleElm327Component::send_command).
std::string normalize_command(const std::string &cmd);

// Parses a raw ELM327 notification chunk (everything received between two
// '>' prompt bytes) into the assembled response bytes.
//
// Handles:
//  - CR/LF-delimited lines, with leading/trailing whitespace trimmed.
//  - Command-echo suppression: a line matching `last_sent_command_normalized`
//    (after normalize_command) is skipped.
//  - Multi-line hex-indexed frames, e.g. "0: 62 E0 02 ..." through
//    "F: ...". The index before ':' may be any single hex digit (0-9, A-F,
//    a-f) — ELM327 adapters use hexadecimal sequence indices once a
//    response spans more than 10 lines.
//  - Plain single-line hex responses (no index prefix).
//
// Returns the assembled bytes, or an empty vector if fewer than 4 hex
// characters (2 bytes) were found.
std::vector<uint8_t> parse_response_bytes(const std::string &response,
                                           const std::string &last_sent_command_normalized);

}  // namespace ble_elm327
}  // namespace esphome
