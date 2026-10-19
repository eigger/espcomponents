#include "response_parser.h"

#include <algorithm>
#include <cctype>

namespace esphome {
namespace ble_elm327 {

std::string normalize_command(const std::string &cmd) {
  std::string compact;
  compact.reserve(cmd.size());
  for (char c : cmd) {
    if (std::isspace(static_cast<unsigned char>(c))) continue;  // remove internal spaces too
    compact.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return compact;
}

std::vector<uint8_t> parse_response_bytes(const std::string &response,
                                           const std::string &last_sent_command_normalized) {
  // Split response by carriage return / newline
  std::vector<std::string> lines;
  size_t start = 0;
  while (start < response.size()) {
    size_t end = response.find_first_of("\r\n", start);
    if (end == std::string::npos) {
      lines.push_back(response.substr(start));
      break;
    }
    if (end > start) {
      lines.push_back(response.substr(start, end - start));
    }
    start = end + 1;
  }

  std::string multiline_hex;
  std::string singleline_hex;

  for (const auto &raw_line : lines) {
    // Trim leading/trailing whitespace
    std::string line = raw_line;
    line.erase(line.begin(), std::find_if(line.begin(), line.end(), [](unsigned char ch) {
      return !std::isspace(ch);
    }));
    line.erase(std::find_if(line.rbegin(), line.rend(), [](unsigned char ch) {
      return !std::isspace(ch);
    }).base(), line.end());

    if (line.empty()) continue;

    // Check if it's the command echo
    if (normalize_command(line) == last_sent_command_normalized) {
      continue;
    }

    // Check if it's a multiline frame line (e.g. "0: 62 E0 02..." or, once a
    // response spans more than 10 lines, "A: ..." through "F: ...").
    bool is_multiline_frame = false;
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos && colon_pos > 0) {
      bool all_hex = true;
      for (size_t i = 0; i < colon_pos; i++) {
        if (!std::isxdigit(static_cast<unsigned char>(line[i]))) {
          all_hex = false;
          break;
        }
      }
      if (all_hex) {
        is_multiline_frame = true;
      }
    }

    if (is_multiline_frame) {
      std::string hex_part = line.substr(colon_pos + 1);
      for (char c : hex_part) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
          multiline_hex += c;
        }
      }
    } else {
      std::string hex_part;
      for (char c : line) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
          hex_part += c;
        }
      }
      if (hex_part.size() > 3) {
        singleline_hex += hex_part;
      }
    }
  }

  std::string hex = multiline_hex.empty() ? singleline_hex : multiline_hex;

  // Must have at least 4 hex chars (1-byte response code + 1-byte PID/data)
  if (hex.size() < 4) return {};

  // Parse consecutive 2-char groups into bytes
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i + 1 < hex.size(); i += 2)
    bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));

  return bytes;
}

static std::string to_upper(const std::string &in) {
  std::string out;
  out.reserve(in.size());
  for (char c : in) out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
  return out;
}

static const char *const BUS_ERROR_LABELS[] = {
    "NO DATA",  "CAN ERROR", "BUS ERROR", "BUS BUSY", "BUS INIT ERROR", "DATA ERROR",
    "FB ERROR", "RX ERROR",  "UNABLE TO CONNECT",
};

static const char *const ADAPTER_ERROR_LABELS[] = {
    "STOPPED", "BUFFER FULL", "LV RESET", "ACT ALERT",
};

std::string classify_error_response(const std::string &response,
                                    const std::string &last_sent_command_normalized) {
  size_t start = 0;
  while (start <= response.size()) {
    size_t end = response.find_first_of("\r\n", start);
    if (end == std::string::npos) end = response.size();
    std::string upper = to_upper(response.substr(start, end - start));
    start = end + 1;

    size_t first = upper.find_first_not_of(" \t");
    if (first == std::string::npos) continue;
    size_t last = upper.find_last_not_of(" \t");
    upper = upper.substr(first, last - first + 1);

    if (!last_sent_command_normalized.empty() && normalize_command(upper) == last_sent_command_normalized) continue;

    if (upper == "?") return "?";
    // "BUS INIT: ...ERROR" (ISO 9141 / KWP slow or fast init failed)
    if (upper.find("BUS INIT") != std::string::npos) {
      if (upper.find("ERROR") != std::string::npos) return "BUS INIT ERROR";
      continue;  // "BUS INIT: ...OK" / in progress
    }
    for (const char *label : BUS_ERROR_LABELS) {
      if (upper.find(label) != std::string::npos) return label;
    }
    for (const char *label : ADAPTER_ERROR_LABELS) {
      if (upper.find(label) != std::string::npos) return label;
    }
    // "ERRxx" internal error codes (e.g. ERR94)
    if (upper.size() >= 4 && upper.compare(0, 3, "ERR") == 0 &&
        std::isdigit(static_cast<unsigned char>(upper[3]))) {
      return upper;
    }
  }
  return "";
}

bool is_bus_error(const std::string &error) {
  for (const char *label : BUS_ERROR_LABELS) {
    if (error == label) return true;
  }
  return false;
}

bool counts_as_link_failure(const std::string &error, const std::string &last_sent_command_normalized,
                            bool follows_timeout) {
  if (error.empty() || is_bus_error(error)) return false;
  if (error == "STOPPED" && follows_timeout) return false;
  if (error == "?") return last_sent_command_normalized.compare(0, 2, "at") != 0;
  return true;
}

TimeoutCheck check_response_timeout(bool auto_recovery, uint32_t active_timeout_ms, bool search_in_progress,
                                    uint32_t since_write_ms, uint32_t since_activity_ms) {
  TimeoutCheck r{false, false, active_timeout_ms, active_timeout_ms};
  if (!auto_recovery) {
    r.expired = since_write_ms > active_timeout_ms;
    return r;
  }
  if (search_in_progress && r.timeout_ms < SEARCH_RESPONSE_TIMEOUT_MS) r.timeout_ms = SEARCH_RESPONSE_TIMEOUT_MS;
  r.hard_limit_ms = 2 * r.timeout_ms > ABSOLUTE_RESPONSE_LIMIT_MS ? 2 * r.timeout_ms : ABSOLUTE_RESPONSE_LIMIT_MS;
  const bool idle = since_activity_ms > r.timeout_ms;
  r.hard_limit_hit = !idle && since_write_ms > r.hard_limit_ms;
  r.expired = idle || r.hard_limit_hit;
  return r;
}

uint32_t reconnect_backoff_ms(uint8_t reconnects) {
  static const uint32_t BASE_MS = 30000;
  static const uint32_t MAX_MS = 600000;
  if (reconnects == 0) return 0;
  uint8_t shift = reconnects - 1;
  if (shift > 5) shift = 5;
  uint32_t ms = BASE_MS << shift;
  return ms > MAX_MS ? MAX_MS : ms;
}

bool pre_command_reply_invalidates_header(const std::string &error) {
  return !error.empty() && error != "?" && !is_bus_error(error);
}

bool pre_commands_applied(const std::vector<std::string> &device_pre_commands,
                          const std::vector<std::string> &current_pre_commands) {
  return device_pre_commands == current_pre_commands;
}

bool command_resets_adapter(const std::string &normalized_command) {
  return normalized_command == "atz" || normalized_command == "atws" || normalized_command == "atd";
}

bool is_protocol_search_in_progress(const std::string &partial_response) {
  std::string upper = to_upper(partial_response);
  return upper.find("SEARCHING") != std::string::npos || upper.find("BUS INIT") != std::string::npos;
}

}  // namespace ble_elm327
}  // namespace esphome
