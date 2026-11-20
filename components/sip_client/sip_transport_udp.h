#pragma once
#include <memory>
#include <vector>
#include "esphome/components/socket/socket.h"
#include "sip_transport.h"

namespace esphome {
namespace sip_client {

class UdpTransport : public SipTransport {
 public:
  bool open(const std::string &server, uint16_t port) override;
  void close() override;
  bool is_open() const override { return this->socket_ != nullptr; }
  bool connected() const override { return this->socket_ != nullptr; }
  bool send(const std::string &msg) override;
  bool poll(std::string &out) override;
  const char *via_token() const override { return "UDP"; }
  std::string via_sent_by() const override;
  std::string contact_host_port() const override { return this->via_sent_by(); }
  const char *contact_transport_param() const override { return ""; }
  uint32_t generation() const override { return 0; }
  std::string local_ip() const override { return this->local_ip_; }

 protected:
  std::unique_ptr<socket::Socket> socket_{nullptr};
  std::string local_ip_;
  uint16_t local_port_{0};
  std::vector<uint8_t> recv_buf_;
};

}  // namespace sip_client
}  // namespace esphome
