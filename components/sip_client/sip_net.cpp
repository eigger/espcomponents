#include "sip_net.h"
#include "esphome/components/network/util.h"

namespace esphome {
namespace sip_client {

std::string local_interface_ipv4() {
  for (auto &addr : network::get_ip_addresses()) {
    if (addr.is_set() && addr.is_ip4()) {
      char ip_buf[network::IP_ADDRESS_BUFFER_SIZE];
      return std::string(addr.str_to(ip_buf));
    }
  }
  return "";
}

}  // namespace sip_client
}  // namespace esphome
