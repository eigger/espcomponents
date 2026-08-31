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

// Value of "Signal=<token>" in a dtmf-relay body. The name is matched
// case-insensitively; "Duration=" and friends are left alone because the '='
// must follow the name.
std::string signal_token(const std::string &body) {
  std::string lower = to_lower(body);
  for (size_t p = lower.find("signal"); p != std::string::npos; p = lower.find("signal", p + 6)) {
    size_t i = p + 6;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) i++;
    if (i >= body.size() || body[i] != '=') continue;
    i++;
    while (i < body.size() && (body[i] == ' ' || body[i] == '\t')) i++;
    std::string token;
    while (i < body.size() && !std::isspace((unsigned char) body[i])) token += body[i++];
    return token;
  }
  return "";
}

}  // namespace

char parse_dtmf_info(const std::string &content_type, const std::string &body) {
  // Content-Type may carry parameters, e.g. "application/dtmf-relay;charset=utf-8".
  std::string type = to_lower(trim(content_type.substr(0, content_type.find(';'))));
  if (type == "application/dtmf-relay") return signal_token_to_char(signal_token(body));
  if (type == "application/dtmf") return signal_token_to_char(trim(body));
  return 0;
}

}  // namespace sip_client
}  // namespace esphome
