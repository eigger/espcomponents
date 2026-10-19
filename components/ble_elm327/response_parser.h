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

// Detects ELM327 status / error replies in a response (everything received
// before the '>' prompt). Returns a short label such as "NO DATA", "STOPPED",
// "?", "BUFFER FULL", "CAN ERROR", "UNABLE TO CONNECT" or "ERRxx", or an empty
// string when the response contains no error line. The command echo line is
// ignored.
std::string classify_error_response(const std::string &response,
                                    const std::string &last_sent_command_normalized);

// True for replies that describe the vehicle bus / ECU side (NO DATA, CAN
// ERROR, BUS ERROR, BUS INIT ERROR, UNABLE TO CONNECT, DATA ERROR, ...). These
// are normal with the ignition off, so they do not count as adapter failures.
bool is_bus_error(const std::string &error);

// True if an error reply indicates an adapter / link problem that should count
// toward the consecutive-failure streak: STOPPED, BUFFER FULL, ERRxx, LV RESET,
// ACT ALERT, and "?" in reply to a non-AT command ("?" to an AT command only
// means that command is unsupported by this adapter).
//
// `follows_timeout` is true when the command was the first one written after
// the previous command timed out. A "STOPPED" then only means the adapter
// aborted the earlier, still-running request (e.g. a slow ATSP0 protocol
// search) when our next command arrived, so it is not counted.
bool counts_as_link_failure(const std::string &error, const std::string &last_sent_command_normalized,
                            bool follows_timeout = false);

// Response timeout decision for the command in flight.
//
// auto_recovery == false (default, same as earlier versions): the command
// times out `active_timeout_ms` after the write.
// auto_recovery == true: the timer restarts with every received byte; while a
// protocol search is reported the idle timeout is at least
// SEARCH_RESPONSE_TIMEOUT_MS; a hard limit of max(ABSOLUTE_RESPONSE_LIMIT_MS,
// 2 x idle timeout) from the write always applies.
static const uint32_t SEARCH_RESPONSE_TIMEOUT_MS = 20000;
static const uint32_t ABSOLUTE_RESPONSE_LIMIT_MS = 30000;

struct TimeoutCheck {
  bool expired;
  bool hard_limit_hit;     // expired by the hard limit (data kept arriving)
  uint32_t timeout_ms;     // idle timeout that applied
  uint32_t hard_limit_ms;  // hard limit that applied (== timeout_ms without auto_recovery)
};

TimeoutCheck check_response_timeout(bool auto_recovery, uint32_t active_timeout_ms, bool search_in_progress,
                                    uint32_t since_write_ms, uint32_t since_activity_ms);

// Minimum time between two recovery reconnects after `reconnects` recovery
// reconnects in a row: 30 s, 60 s, 120 s, ... capped at 10 min. 0 for 0.
uint32_t reconnect_backoff_ms(uint8_t reconnects);

// True if an error reply to a pre_command (e.g. ATSH) means the header may not
// be applied, so it must be re-sent before the next PID request. "?" (command
// not supported by this adapter) and vehicle-side errors leave the adapter
// state unchanged, so the header stays sticky and is not re-sent every poll.
// STOPPED always counts here (the command was aborted), even right after a
// timeout.
bool pre_command_reply_invalidates_header(const std::string &error);

// True if a queued PID request may be sent: the device's pre_commands match
// the header currently tracked as applied on the adapter. False only after the
// tracked header was cleared (pre-command failure, adapter reset) between
// queueing the device's pre_commands and its PID request.
bool pre_commands_applied(const std::vector<std::string> &device_pre_commands,
                          const std::vector<std::string> &current_pre_commands);

// True for commands that reset the adapter's settings, including any ATSH
// header: ATZ (reset), ATWS (warm start), ATD (defaults). Expects a
// normalize_command() result.
bool command_resets_adapter(const std::string &normalized_command);

// True while a partial reply shows the adapter is still searching for /
// initializing the OBD protocol ("SEARCHING...", "BUS INIT: ...").
bool is_protocol_search_in_progress(const std::string &partial_response);

}  // namespace ble_elm327
}  // namespace esphome
