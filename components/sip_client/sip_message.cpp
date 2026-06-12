#include "sip_message.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace esphome {
namespace sip_client {

static std::string to_lower(std::string s) {
  for (auto &c : s) c = (char) std::tolower((unsigned char) c);
  return s;
}

static std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  size_t e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

std::string SipMessage::header(const std::string &name) const {
  auto it = headers.find(to_lower(name));
  return it == headers.end() ? std::string() : it->second;
}

bool SipMessage::has_header(const std::string &name) const {
  return headers.find(to_lower(name)) != headers.end();
}

SipMessage parse_sip_message(const std::string &raw) {
  SipMessage msg;
  size_t header_end = raw.find("\r\n\r\n");
  std::string head = (header_end == std::string::npos) ? raw : raw.substr(0, header_end);
  if (header_end != std::string::npos) msg.body = raw.substr(header_end + 4);

  size_t pos = 0;
  bool first = true;
  while (pos < head.size()) {
    size_t eol = head.find("\r\n", pos);
    std::string line = head.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    pos = (eol == std::string::npos) ? head.size() : eol + 2;

    if (first) {
      first = false;
      // Request line "INVITE sip:... SIP/2.0" or status line "SIP/2.0 200 OK".
      if (line.compare(0, 8, "SIP/2.0 ") == 0) {
        msg.is_request = false;
        msg.status_code = std::atoi(line.substr(8, 3).c_str());
        if (line.size() > 12) msg.reason = trim(line.substr(12));
      } else {
        msg.is_request = true;
        size_t sp1 = line.find(' ');
        size_t sp2 = line.rfind(' ');
        if (sp1 != std::string::npos) {
          msg.method = line.substr(0, sp1);
          if (sp2 != std::string::npos && sp2 > sp1)
            msg.request_uri = trim(line.substr(sp1 + 1, sp2 - sp1 - 1));
        }
      }
      continue;
    }

    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = to_lower(trim(line.substr(0, colon)));
    std::string value = trim(line.substr(colon + 1));
    // Keep the first occurrence (topmost Via, etc.).
    if (msg.headers.find(name) == msg.headers.end()) msg.headers[name] = value;
  }
  return msg;
}

SdpInfo parse_sdp(const std::string &body) {
  SdpInfo info;
  size_t pos = 0;
  while (pos < body.size()) {
    size_t eol = body.find('\n', pos);
    std::string line = body.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    pos = (eol == std::string::npos) ? body.size() : eol + 1;
    line = trim(line);
    if (line.empty()) continue;

    if (line.compare(0, 7, "c=IN IP") == 0) {
      // c=IN IP4 192.168.0.5
      size_t sp = line.rfind(' ');
      if (sp != std::string::npos) info.connection_ip = trim(line.substr(sp + 1));
    } else if (line.compare(0, 8, "m=audio ") == 0) {
      info.valid = true;
      info.audio_port = (uint16_t) std::atoi(line.substr(8).c_str());
      // Default well-known payload types if present in the m= list.
      std::string rest = line.substr(8);
      if (rest.find(" 0") != std::string::npos || rest.find(" 0 ") != std::string::npos)
        info.pcmu_pt = 0;
      if (rest.find(" 8") != std::string::npos) info.pcma_pt = 8;
    } else if (line.compare(0, 9, "a=rtpmap:") == 0) {
      // a=rtpmap:101 telephone-event/8000
      int pt = std::atoi(line.substr(9).c_str());
      std::string lower = to_lower(line);
      if (lower.find("telephone-event") != std::string::npos) {
        info.telephone_event_pt = pt;
      } else if (lower.find("pcmu") != std::string::npos) {
        info.pcmu_pt = pt;
      } else if (lower.find("pcma") != std::string::npos) {
        info.pcma_pt = pt;
      }
    }
  }
  return info;
}

std::string auth_param(const std::string &header_value, const std::string &key) {
  std::string lower = to_lower(header_value);
  std::string needle = to_lower(key);
  size_t kpos = 0;
  while ((kpos = lower.find(needle, kpos)) != std::string::npos) {
    size_t after = kpos + needle.size();
    // Make sure this is the parameter name (followed by optional space then '=').
    size_t eq = after;
    while (eq < header_value.size() && (header_value[eq] == ' ' || header_value[eq] == '\t')) eq++;
    if (eq >= header_value.size() || header_value[eq] != '=') {
      kpos = after;
      continue;
    }
    // Ensure the char before key is a delimiter (start, space, comma).
    if (kpos > 0) {
      char prev = header_value[kpos - 1];
      if (prev != ' ' && prev != ',' && prev != '\t') {
        kpos = after;
        continue;
      }
    }
    size_t vpos = eq + 1;
    while (vpos < header_value.size() && (header_value[vpos] == ' ' || header_value[vpos] == '\t')) vpos++;
    if (vpos < header_value.size() && header_value[vpos] == '"') {
      size_t end = header_value.find('"', vpos + 1);
      if (end == std::string::npos) return "";
      return header_value.substr(vpos + 1, end - vpos - 1);
    }
    size_t end = header_value.find_first_of(", \t", vpos);
    return header_value.substr(vpos, end == std::string::npos ? std::string::npos : end - vpos);
  }
  return "";
}

std::string gen_random_hex(size_t bytes) {
  static const char *hex = "0123456789abcdef";
  std::string out;
  out.reserve(bytes * 2);
  for (size_t i = 0; i < bytes * 2; i++) out.push_back(hex[std::rand() & 0x0F]);
  return out;
}

std::string gen_branch() { return "z9hG4bK" + gen_random_hex(8); }
std::string gen_tag() { return gen_random_hex(6); }
std::string gen_call_id(const std::string &host) { return gen_random_hex(12) + "@" + host; }

}  // namespace sip_client
}  // namespace esphome
