#pragma once
#include <string>

namespace esphome {
namespace sip_client {

// First real interface IPv4 address in dotted-quad form, or "" if none yet.
// Used for the SDP c= line (media is plain UDP regardless of signaling
// transport) and, for UDP, as the Via/Contact address.
std::string local_interface_ipv4();

}  // namespace sip_client
}  // namespace esphome
