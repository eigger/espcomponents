#pragma once
#include <cstdint>
#include <string>

// Deliberately free of ESPHome includes so the pure header-building helpers
// below can be unit-tested natively (tests/native/sip_sdp).

namespace esphome {
namespace sip_client {

// Signaling transport underneath SipClient (RFC 3261 UDP today; RFC 7118
// WebSocket next). RTP media never goes through this.
//
// Lifecycle: open() may complete asynchronously (a WebSocket needs a TCP/TLS
// connect plus an HTTP upgrade). is_open() means "a connection attempt is in
// flight or established, don't call open() again"; connected() means "send()
// will work now". For UDP both flip together inside open().
class SipTransport {
 public:
  virtual ~SipTransport() = default;

  // Start (or, for UDP, fully complete) connecting to the server.
  virtual bool open(const std::string &server, uint16_t port) = 0;
  virtual void close() = 0;
  virtual bool is_open() const = 0;
  // True once messages can be sent; false again if the link drops.
  virtual bool connected() const = 0;

  virtual bool send(const std::string &msg) = 0;
  // Non-blocking; returns true and one complete SIP message per call.
  virtual bool poll(std::string &out) = 0;

  // Via transport token: "UDP", later "WS" / "WSS".
  virtual const char *via_token() const = 0;
  // Via sent-by: host:port for UDP, a random `<token>.invalid` for WS (RFC 7118 5.2).
  virtual std::string via_sent_by() const = 0;
  // Contact URI host[:port] and extra URI params (";transport=ws"; empty for UDP).
  virtual std::string contact_host_port() const = 0;
  virtual const char *contact_transport_param() const = 0;
  // Reliable transports skip SIP-level retransmission.
  virtual bool reliable() const = 0;

  // Real interface IPv4 address, used for the SDP c= line (media is plain UDP
  // even when signaling is not).
  virtual std::string local_ip() const = 0;
};

// "Via: SIP/2.0/<token> <sent-by>;branch=<branch>;rport\r\n"
inline std::string sip_via_header(const char *token, const std::string &sent_by,
                                  const std::string &branch) {
  return std::string("Via: SIP/2.0/") + token + " " + sent_by + ";branch=" + branch + ";rport\r\n";
}

// "<sip:user@host:port[;transport=x]>"
inline std::string sip_contact_uri(const std::string &user, const std::string &host_port,
                                   const char *transport_param) {
  return "<sip:" + user + "@" + host_port + transport_param + ">";
}

}  // namespace sip_client
}  // namespace esphome
