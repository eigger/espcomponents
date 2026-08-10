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

}  // namespace ble_elm327
}  // namespace esphome
