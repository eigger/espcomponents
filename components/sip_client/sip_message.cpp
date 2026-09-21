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

static std::string resolve_header_name(const std::string &name) {
  if (name == "a") return "accept-contact";
  if (name == "b") return "referred-by";
  if (name == "c") return "content-type";
  if (name == "e") return "content-encoding";
  if (name == "f") return "from";
  if (name == "i") return "call-id";
  if (name == "k") return "supported";
  if (name == "l") return "content-length";
  if (name == "m") return "contact";
  if (name == "o") return "event";
  if (name == "r") return "refer-to";
  if (name == "s") return "subject";
  if (name == "t") return "to";
  if (name == "u") return "allow-events";
  if (name == "v") return "via";
  return name;
}

// Headers that may legally repeat and whose values form one ordered list.
static bool is_comma_list_header(const std::string &name) {
  return name == "via" || name == "record-route" || name == "route" || name == "service-route";
}

static std::vector<std::string> split_ws(const std::string &s) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < s.size()) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i >= s.size()) break;
    size_t j = i;
    while (j < s.size() && s[j] != ' ' && s[j] != '\t') j++;
    out.push_back(s.substr(i, j - i));
    i = j;
  }
  return out;
}

// Encoding name from "name/rate[/channels]", lowercased.
static std::string rtpmap_name(const std::string &enc) {
  size_t slash = enc.find('/');
  return slash == std::string::npos ? enc : enc.substr(0, slash);
}

bool SdpInfo::has_payload_type(int pt) const {
  for (int p : this->payload_types) {
    if (p == pt) return true;
  }
  return false;
}

std::string SipMessage::header(const std::string &name) const {
  auto it = headers.find(resolve_header_name(to_lower(name)));
  return it == headers.end() ? std::string() : it->second;
}

bool SipMessage::has_header(const std::string &name) const {
  return headers.find(resolve_header_name(to_lower(name))) != headers.end();
}

SipMessage parse_sip_message(const std::string &raw) {
  SipMessage msg;
  size_t header_end = raw.find("\r\n\r\n");
  std::string head = (header_end == std::string::npos) ? raw : raw.substr(0, header_end);
  if (header_end != std::string::npos) msg.body = raw.substr(header_end + 4);

  size_t pos = 0;
  bool first = true;
  std::string last_name;
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

    if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
      // Header folding: continuation of the previous header line.
      if (!last_name.empty()) msg.headers[last_name] += " " + trim(line);
      continue;
    }

    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = resolve_header_name(to_lower(trim(line.substr(0, colon))));
    std::string value = trim(line.substr(colon + 1));
    last_name = name;
    auto it = msg.headers.find(name);
    if (it == msg.headers.end()) {
      msg.headers[name] = value;
    } else if (is_comma_list_header(name)) {
      // Repeated rows and one comma-separated row are equivalent on the wire;
      // keep the proxy path in order (topmost Via first).
      it->second += ", " + value;
    }
    // Any other repeated header keeps its first occurrence.
  }
  return msg;
}

std::vector<std::string> split_header_values(const std::string &value) {
  std::vector<std::string> values;
  size_t start = 0;
  int angle_depth = 0;
  bool quoted = false;
  bool escaped = false;
  for (size_t i = 0; i < value.size(); i++) {
    char c = value[i];
    if (escaped) {
      escaped = false;
    } else if (quoted && c == '\\') {
      escaped = true;
    } else if (c == '"') {
      quoted = !quoted;
    } else if (!quoted && c == '<') {
      angle_depth++;
    } else if (!quoted && c == '>' && angle_depth > 0) {
      angle_depth--;
    } else if (!quoted && angle_depth == 0 && c == ',') {
      std::string item = trim(value.substr(start, i - start));
      if (!item.empty()) values.push_back(item);
      start = i + 1;
    }
  }
  std::string item = trim(value.substr(start));
  if (!item.empty()) values.push_back(item);
  return values;
}

std::string extract_angle_uri(const std::string &value) {
  size_t lt = value.find('<');
  size_t gt = value.find('>');
  if (lt != std::string::npos && gt != std::string::npos && gt > lt) {
    return value.substr(lt + 1, gt - lt - 1);
  }
  std::string stripped = trim(value);
  if (stripped.rfind("sip", 0) == 0) {
    return stripped;
  }
  return "";
}

std::string via_branch(const std::string &via) {
  // Only the top Via names our transaction; later ones belong to proxies.
  std::string top = via.substr(0, via.find(','));
  std::string lower = to_lower(top);
  size_t p = lower.find(";branch=");
  if (p == std::string::npos) return "";
  p += 8;
  size_t e = top.find_first_of(";, \t\r\n", p);
  return top.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

bool is_loose_route(const std::string &route) {
  std::string uri = extract_angle_uri(route);
  if (uri.empty()) uri = trim(route);
  std::string lower = to_lower(uri);
  // Only URI parameters count (RFC 3261 §16.4). They sit between the
  // hostport and the "?" that starts the headers part; cut there first so an
  // '@' inside a header value is not mistaken for the userinfo separator,
  // then skip the userinfo (";lr" before '@' is not a parameter).
  size_t end = lower.find('?');
  if (end == std::string::npos) end = lower.size();
  size_t p = lower.rfind('@', end);
  p = (p == std::string::npos) ? 0 : p + 1;
  // ";lr" must be a whole parameter: ";lr", ";lr=...", ";lr;..." or ";lr?"
  // count, but ";lrx" must not.
  while ((p = lower.find(";lr", p)) != std::string::npos && p < end) {
    size_t after = p + 3;
    if (after >= end) return true;
    char c = lower[after];
    if (c == ';' || c == '=' || c == '?') return true;
    p = after;
  }
  return false;
}

void apply_route_set(const std::vector<std::string> &routes, std::string &target,
                     std::string &route_block) {
  route_block.clear();
  std::vector<std::string> active;
  for (const auto &r : routes) {
    if (!trim(r).empty()) active.push_back(r);
  }
  if (active.empty()) return;
  // One Route header per hop (like the Record-Route echo): some proxies only
  // read the first value of a comma-joined list.
  auto emit = [&route_block](const std::vector<std::string> &v) {
    for (const auto &r : v) route_block += "Route: " + r + "\r\n";
  };
  if (is_loose_route(active[0])) {
    // Loose router: remote target stays in the Request-URI, whole route set
    // travels as Route headers.
    emit(active);
    return;
  }
  // Strict router: it takes the Request-URI; the remote target moves to the
  // tail of the route set so it is not lost.
  std::vector<std::string> remaining(active.begin() + 1, active.end());
  if (!target.empty()) remaining.push_back("<" + target + ">");
  emit(remaining);
  std::string first = extract_angle_uri(active[0]);
  target = first.empty() ? active[0] : first;
}

static void derive_codec_pts_(SdpInfo &info) {
  for (int pt : info.payload_types) {
    std::string name;
    auto it = info.rtpmap.find(pt);
    if (it != info.rtpmap.end()) {
      name = rtpmap_name(it->second);
    } else if (pt == 0) {
      name = "pcmu";  // RFC 3551 static
    } else if (pt == 8) {
      name = "pcma";
    } else if (pt == 9) {
      name = "g722";  // RFC 3551 static
    }

    if (name == "telephone-event") {
      if (info.telephone_event_pt < 0) info.telephone_event_pt = pt;
    } else if (name == "pcmu") {
      if (info.pcmu_pt < 0) info.pcmu_pt = pt;
    } else if (name == "pcma") {
      if (info.pcma_pt < 0) info.pcma_pt = pt;
    } else if (name == "g722") {
      if (info.g722_pt < 0) info.g722_pt = pt;
    }
  }
}

static bool is_all_digits_(const std::string &s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  }
  return true;
}

SdpInfo parse_sdp(const std::string &body) {
  SdpInfo info;
  // Track the current m= section so video (or a second audio) cannot pollute
  // audio fmt / rtpmap / media-level c= fields.
  bool seen_m_line = false;
  bool in_audio_section = false;
  size_t pos = 0;
  while (pos < body.size()) {
    size_t eol = body.find('\n', pos);
    std::string line = body.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    pos = (eol == std::string::npos) ? body.size() : eol + 1;
    line = trim(line);
    if (line.empty()) continue;

    if (line.compare(0, 2, "m=") == 0) {
      seen_m_line = true;
      if (line.compare(0, 8, "m=audio ") == 0 && !info.valid) {
        // First audio m-line only.
        in_audio_section = true;
        info.valid = true;
        auto tokens = split_ws(line.substr(8));
        // Port may be "<port>/<number of ports>" (RFC 4566); atoi stops at '/'.
        if (!tokens.empty())
          info.audio_port = (uint16_t) std::atoi(tokens[0].c_str());
        for (size_t t = 2; t < tokens.size(); t++) {
          // Tokenize — never substring-search (" 9" must not match PT 96..99).
          // Skip non-numeric fmt tokens (atoi would silently invent PT 0).
          if (!is_all_digits_(tokens[t])) continue;
          info.payload_types.push_back(std::atoi(tokens[t].c_str()));
        }
      } else {
        in_audio_section = false;  // m=video, second m=audio, etc.
      }
    } else if (line.compare(0, 7, "c=IN IP") == 0) {
      // Session-level c= (before any m=) or audio media-level c= only.
      if (!seen_m_line || in_audio_section) {
        size_t sp = line.rfind(' ');
        if (sp != std::string::npos) info.connection_ip = trim(line.substr(sp + 1));
      }
    } else if (line.compare(0, 9, "a=rtpmap:") == 0) {
      if (!in_audio_section) continue;
      // a=rtpmap:101 telephone-event/8000
      int pt = std::atoi(line.substr(9).c_str());
      size_t sp = line.find(' ');
      if (sp == std::string::npos) continue;
      info.rtpmap[pt] = to_lower(trim(line.substr(sp + 1)));
    }
  }
  derive_codec_pts_(info);
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
