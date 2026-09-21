#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace esphome {
namespace sip_client {

// Parsed view of an incoming SIP message (request or response).
struct SipMessage {
  bool is_request{false};
  std::string method;       // request only, e.g. "INVITE"
  std::string request_uri;  // request only
  int status_code{0};       // response only
  std::string reason;       // response only

  // Common headers (raw values, leading/trailing space trimmed). Names are
  // stored lowercase in `headers`. Repeated list headers (Via, Record-Route,
  // Route, Service-Route) are joined with ", " in wire order so the full
  // proxy path survives; every other repeated header keeps its first value.
  std::map<std::string, std::string> headers;
  std::string body;

  std::string header(const std::string &name) const;
  bool has_header(const std::string &name) const;
};

// Parsed SDP media description (audio only).
struct SdpInfo {
  bool valid{false};
  std::string connection_ip;      // c=IN IP4 <addr>
  uint16_t audio_port{0};         // m=audio <port> ...
  std::vector<int> payload_types; // fmt list from m= line, in order
  // pt -> lowercase "name/rate" from a=rtpmap (e.g. "pcmu/8000")
  std::map<int, std::string> rtpmap;
  // Convenience: first matching audio PT in payload_types (-1 if absent).
  int pcmu_pt{-1};
  int pcma_pt{-1};
  int g722_pt{-1};
  int telephone_event_pt{-1};  // dynamic PT for RFC2833, -1 if absent

  bool has_payload_type(int pt) const;
};

SipMessage parse_sip_message(const std::string &raw);
SdpInfo parse_sdp(const std::string &body);

// Split a comma-list header value (Via, Record-Route, Route, ...) into its
// field-values without splitting commas nested in <...> or quoted strings.
std::vector<std::string> split_header_values(const std::string &value);

// URI inside <...>, or the trimmed value itself when it is a bare sip: URI.
std::string extract_angle_uri(const std::string &value);

// Branch parameter of the top Via (the transaction the message belongs to).
std::string via_branch(const std::string &via);

// Whether a Record-Route/Route field-value points at a loose router (;lr).
bool is_loose_route(const std::string &route);

// Extract a quoted-or-token parameter from an auth header value, e.g.
// auth_param("Digest realm=\"asterisk\", nonce=\"abc\"", "nonce") -> "abc".
std::string auth_param(const std::string &header_value, const std::string &key);

// Random identifiers for SIP dialogs.
std::string gen_random_hex(size_t bytes);
std::string gen_branch();   // RFC 3261 magic-cookie branch
std::string gen_tag();
std::string gen_call_id(const std::string &host);

}  // namespace sip_client
}  // namespace esphome
