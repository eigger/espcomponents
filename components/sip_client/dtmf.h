#pragma once
#include <cstdint>
#include <string>

namespace esphome {
namespace sip_client {

// RFC 4733 event code -> DTMF character. Returns 0 for codes we do not expose
// (16 = flash and up), which callers drop.
inline char dtmf_event_to_char(uint8_t event) {
  if (event <= 9) return (char) ('0' + event);
  if (event == 10) return '*';
  if (event == 11) return '#';
  if (event <= 15) return (char) ('A' + (event - 12));
  return 0;
}

inline bool is_dtmf_char(char c) {
  return (c >= '0' && c <= '9') || c == '*' || c == '#' || (c >= 'A' && c <= 'D');
}

// De-duplication for received RFC 4733 telephone-event packets. Every packet of
// one digit repeats the same RTP timestamp, and the end packet is normally sent
// three times, so firing per packet would repeat the digit. Firing on the
// marker bit alone is not an option either: Yealink DECT handsets and the 3CX
// Android app send the event without it. Fire once per (event, timestamp).
class DtmfRxDedup {
 public:
  // Digit for a newly started event, or 0 for a repeat packet of the event
  // already reported (or an event code we do not expose).
  char feed(uint8_t event, uint32_t timestamp) {
    if (this->started_ && event == this->last_event_ && timestamp == this->last_timestamp_)
      return 0;
    this->started_ = true;
    this->last_event_ = event;
    this->last_timestamp_ = timestamp;
    return dtmf_event_to_char(event);
  }

  void reset() { this->started_ = false; }

 protected:
  bool started_{false};
  uint8_t last_event_{0};
  uint32_t last_timestamp_{0};
};

// DTMF carried in a SIP INFO body, as sent by the 3CX Android app and by desk
// phones configured for "SIP INFO" DTMF. Returns the digit, or 0 when the INFO
// is not a DTMF INFO.
//
//   application/dtmf-relay:  "Signal=1\r\nDuration=160"
//   application/dtmf:        "1"
//
// Senders disagree on how * and # are written -- literally, or as the RFC 4733
// event codes 10 and 11 -- so both spellings are accepted.
char parse_dtmf_info(const std::string &content_type, const std::string &body);

}  // namespace sip_client
}  // namespace esphome
