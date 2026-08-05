#include "sdp_builder.h"

namespace esphome {
namespace sip_client {

namespace {

int static_pt_for(AudioCodecId id) {
  switch (id) {
    case AudioCodecId::PCMU:
      return 0;
    case AudioCodecId::PCMA:
      return 8;
    case AudioCodecId::G722:
      return 9;
  }
  return -1;
}

const char *rtpmap_for(AudioCodecId id) {
  switch (id) {
    case AudioCodecId::PCMU:
      return "PCMU/8000";
    case AudioCodecId::PCMA:
      return "PCMA/8000";
    case AudioCodecId::G722:
      return "G722/8000";
  }
  return nullptr;
}

std::vector<AudioCodecId> default_offer_codecs() {
  return {AudioCodecId::PCMU, AudioCodecId::PCMA};
}

}  // namespace

std::string build_sdp_body(const SdpMediaParams &p) {
  std::string sdp;
  sdp += "v=0\r\n";
  sdp += "o=- " + p.session_id + " " + p.session_id + " IN IP4 " + p.local_ip + "\r\n";
  sdp += "s=esphome\r\n";
  sdp += "c=IN IP4 " + p.local_ip + "\r\n";
  sdp += "t=0 0\r\n";

  if (p.answer) {
    // RFC 3264: answer lists only the selected format(s).
    std::string mline = "m=audio " + std::to_string(p.rtp_port) + " RTP/AVP " +
                        std::to_string(p.answer_pt);
    if (p.dtmf_pt >= 0)
      mline += " " + std::to_string(p.dtmf_pt);
    sdp += mline + "\r\n";
    const char *rtpmap = p.answer_rtpmap != nullptr ? p.answer_rtpmap : "PCMU/8000";
    sdp += "a=rtpmap:" + std::to_string(p.answer_pt) + " " + rtpmap + "\r\n";
    if (p.dtmf_pt >= 0) {
      sdp += "a=rtpmap:" + std::to_string(p.dtmf_pt) + " telephone-event/8000\r\n";
      sdp += "a=fmtp:" + std::to_string(p.dtmf_pt) + " 0-15\r\n";
    }
  } else {
    const auto codecs = p.offer_codecs.empty() ? default_offer_codecs() : p.offer_codecs;
    const int dtmf = p.dtmf_pt >= 0 ? p.dtmf_pt : 101;
    std::string mline = "m=audio " + std::to_string(p.rtp_port) + " RTP/AVP";
    for (AudioCodecId id : codecs) {
      int pt = static_pt_for(id);
      if (pt >= 0) mline += " " + std::to_string(pt);
    }
    mline += " " + std::to_string(dtmf);
    sdp += mline + "\r\n";
    for (AudioCodecId id : codecs) {
      int pt = static_pt_for(id);
      const char *rtpmap = rtpmap_for(id);
      if (pt >= 0 && rtpmap != nullptr)
        sdp += "a=rtpmap:" + std::to_string(pt) + " " + rtpmap + "\r\n";
    }
    sdp += "a=rtpmap:" + std::to_string(dtmf) + " telephone-event/8000\r\n";
    sdp += "a=fmtp:" + std::to_string(dtmf) + " 0-15\r\n";
  }

  sdp += "a=ptime:20\r\n";
  sdp += std::string("a=") + (p.direction != nullptr ? p.direction : "sendrecv") + "\r\n";
  return sdp;
}

}  // namespace sip_client
}  // namespace esphome
