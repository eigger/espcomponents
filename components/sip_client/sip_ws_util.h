#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

// Pure helpers for the WebSocket transport. Deliberately free of ESPHome /
// ESP-IDF includes so they can be unit-tested natively (tests/native/sip_sdp).

namespace esphome {
namespace sip_client {

// RFC 7118 §5.2: the Via sent-by of a WebSocket client is a random token under
// the reserved ".invalid" TLD ("<token>.invalid"), because the real address is
// meaningless to the server (the response retraces the connection). `rand32`
// is any callable returning a uint32_t.
template<typename RandFn> inline std::string sip_random_invalid_host(RandFn &&rand32, size_t len = 10) {
  static const char ALPHABET[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  static const size_t N = sizeof(ALPHABET) - 1;
  std::string out;
  out.reserve(len + 8);
  for (size_t i = 0; i < len; i++) out += ALPHABET[static_cast<uint32_t>(rand32()) % N];
  out += ".invalid";
  return out;
}

// "ws(s)://host:port/path". An IPv6 literal host gets brackets; `path` should
// start with '/' (one is added when missing).
inline std::string sip_ws_uri(bool tls, const std::string &host, uint16_t port, const std::string &path) {
  std::string uri = tls ? "wss://" : "ws://";
  if (host.find(':') != std::string::npos && host.front() != '[')
    uri += "[" + host + "]";
  else
    uri += host;
  uri += ":" + std::to_string(port);
  if (path.empty() || path.front() != '/') uri += "/";
  uri += path;
  return uri;
}

// Reassembles WebSocket data events into whole SIP messages.
//
// esp_websocket_client splits a frame larger than its buffer into several DATA
// events (same opcode, rising payload_offset, constant payload_len), and a
// message may in turn span several frames (TEXT/BINARY then CONTINUATION
// frames, FIN on the last). feed() takes one event and reports when a whole
// message is ready. Control frames (close/ping/pong) never touch the state.
class WsReassembler {
 public:
  enum Result : uint8_t {
    NONE,       // nothing complete yet (or ignored)
    MESSAGE,    // `out` holds a complete message
    OVERSIZED,  // message exceeded max_size and is being discarded
  };

  explicit WsReassembler(size_t max_size = 8192) : max_size_(max_size) {}

  void reset() {
    this->buf_.clear();
    this->active_ = false;
    this->dropping_ = false;
  }

  Result feed(uint8_t op_code, const char *data, int data_len, int payload_len, int payload_offset, bool fin,
              std::string &out) {
    const uint8_t op = op_code & 0x0F;
    if (op != OP_CONT && op != OP_TEXT && op != OP_BINARY) return NONE;  // control frame
    if (data_len < 0) data_len = 0;
    const bool frame_end = payload_len <= 0 || payload_offset + data_len >= payload_len;
    const bool msg_end = frame_end && fin;

    if (this->dropping_) {
      if (msg_end) this->dropping_ = false;
      return NONE;
    }
    if (op != OP_CONT && payload_offset == 0) {
      // First chunk of a new message; anything half-built is abandoned.
      this->buf_.clear();
      this->active_ = true;
    } else if (!this->active_) {
      // Stray continuation with no message in progress: skip the rest of it.
      this->dropping_ = !msg_end;
      return NONE;
    }
    if (this->buf_.size() + static_cast<size_t>(data_len) > this->max_size_) {
      this->buf_.clear();
      this->active_ = false;
      this->dropping_ = !msg_end;
      return OVERSIZED;
    }
    if (data_len > 0) this->buf_.append(data, static_cast<size_t>(data_len));
    if (!msg_end) return NONE;
    this->active_ = false;
    if (this->buf_.empty()) return NONE;
    out = std::move(this->buf_);
    this->buf_.clear();
    return MESSAGE;
  }

  static constexpr uint8_t OP_CONT = 0x0;
  static constexpr uint8_t OP_TEXT = 0x1;
  static constexpr uint8_t OP_BINARY = 0x2;

 protected:
  std::string buf_;
  size_t max_size_;
  bool active_{false};
  bool dropping_{false};
};

}  // namespace sip_client
}  // namespace esphome
