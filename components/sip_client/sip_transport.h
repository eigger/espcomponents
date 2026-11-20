#pragma once
#include <cstdint>
#include <string>

// Deliberately free of ESPHome includes so the pure header-building helpers
// below can be unit-tested natively (tests/native/sip_sdp).

namespace esphome {
namespace sip_client {

// Signaling transport underneath SipClient (RFC 3261 UDP, RFC 7118 WebSocket).
// RTP media never goes through this.
//
// Threading: every method is called from the ESPHome main loop only.
// Implementations that receive on a background task (the WebSocket client's)
// must make the state they share with that task safe themselves (atomics, a
// mutex-guarded queue), and must never block the main loop for long.
//
// via_sent_by() / contact_host_port() must return the same value for the whole
// life of one connection (the PBX ties the registration to it), and a new one
// for a new connection is fine.
//
// Lifecycle: open() may complete asynchronously (a WebSocket needs a TCP/TLS
// connect plus an HTTP upgrade). is_open() means "a connection attempt is in
// flight or established, don't call open() again"; connected() means "send()
// will work now". For UDP both flip together inside open().
class SipTransport {
 public:
  virtual ~SipTransport() = default;

  // Start (or, for UDP, fully complete) connecting to the server.
  // Periodic housekeeping on the main loop (watchdogs, picking up state from a
  // background task). Called once per SipClient::loop() before anything else.
  virtual void loop() {}
  // Called instead of loop() while the device has no network (WiFi down), so
  // implementations can pause liveness tracking that would otherwise count the
  // outage against the link.
  virtual void network_down() {}

  virtual bool open(const std::string &server, uint16_t port) = 0;
  // Tear the connection down (WebSocket: close frame, then stop). open() may
  // be called again afterwards. Called on shutdown and from destructors.
  virtual void close() = 0;
  virtual bool is_open() const = 0;
  // True once messages can be sent; false again if the link drops.
  virtual bool connected() const = 0;
  // Changes every time a new underlying connection is established, so SipClient
  // notices a drop-and-reconnect that happened between two loop() calls (the
  // old registration died with the old connection). Constant for UDP.
  virtual uint32_t generation() const = 0;

  virtual bool send(const std::string &msg) = 0;
  // Non-blocking; returns true and one complete SIP message per call.
  virtual bool poll(std::string &out) = 0;

  // Via transport token: "UDP", "WS" or "WSS".
  virtual const char *via_token() const = 0;
  // Via sent-by: host:port for UDP, a random `<token>.invalid` for WS (RFC 7118 5.2).
  virtual std::string via_sent_by() const = 0;
  // Contact URI host[:port] and extra URI params (";transport=ws"; empty for UDP).
  virtual std::string contact_host_port() const = 0;
  virtual const char *contact_transport_param() const = 0;

  // Real interface IPv4 address, used for the SDP c= line (media is plain UDP
  // even when signaling is not).
  virtual std::string local_ip() const = 0;
};

// "Via: SIP/2.0/<token> <sent-by>;branch=<branch>;rport\r\n"
// ;rport is kept for WS/WSS too: it is harmless there (the server answers over
// the connection the request arrived on, and Asterisk accepts it on any
// transport), and it keeps the UDP wire output byte-identical.
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
