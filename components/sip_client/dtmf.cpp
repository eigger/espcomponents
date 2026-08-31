#include "dtmf.h"

#include <cctype>
#include <cstdlib>

namespace esphome {
namespace sip_client {

namespace {

std::string to_lower(const std::string &s) {
  std::string out = s;
  for (char &c : out) c = (char) std::tolower((unsigned char) c);
  return out;
}

std::string trim(const std::string &s) {
  size_t begin = 0, end = s.size();
  while (begin < end && std::isspace((unsigned char) s[begin])) begin++;
  while (end > begin && std::isspace((unsigned char) s[end - 1])) end--;
  return s.substr(begin, end - begin);
}

// "1" / "10" / "*" -> digit, 0 when the token is not a DTMF signal.
char signal_token_to_char(const std::string &token) {
  if (token.empty()) return 0;
  bool numeric = true;
  for (char c : token) {
    if (c < '0' || c > '9') {
      numeric = false;
      break;
    }
  }
  if (numeric) {
    if (token.size() > 2) return 0;
    return dtmf_event_to_char((uint8_t) std::atoi(token.c_str()));
  }
  if (token.size() != 1) return 0;
  char c = (char) std::toupper((unsigned char) token[0]);
  return is_dtmf_char(c) ? c : 0;
}

bool is_dtmf_content_type(const std::string &content_type) {
  // Empty: gateways that send a body with no Content-Type at all.
  if (trim(content_type).empty()) return true;
  std::string type = to_lower(content_type);
  return type.find("application/dtmf") != std::string::npos ||
         type.find("audio/telephone-event") != std::string::npos;
}

// Key of a "<name>=<value>" body line, lowercased; empty when the line has no
// '=' at all.
std::string line_key(const std::string &line) {
  size_t eq = line.find('=');
  if (eq == std::string::npos) return "";
  return to_lower(trim(line.substr(0, eq)));
}

}  // namespace

char parse_dtmf_info(const std::string &content_type, const std::string &body) {
  if (!is_dtmf_content_type(content_type)) return 0;
  if (body.empty()) return 0;

  // "Signal=1", "d=1" or "dtmf=1" on a line of its own. Other keys, notably
  // "Duration=250", must not be read as a digit.
  size_t pos = 0;
  while (pos < body.size()) {
    size_t eol = body.find('\n', pos);
    std::string line = body.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    std::string key = line_key(line);
    if (key == "signal" || key == "d" || key == "dtmf")
      return signal_token_to_char(trim(line.substr(line.find('=') + 1)));
    if (eol == std::string::npos) break;
    pos = eol + 1;
  }

  // No key/value pair: some devices send just the digit as the whole body.
  return signal_token_to_char(trim(body));
}

}  // namespace sip_client
}  // namespace esphome
