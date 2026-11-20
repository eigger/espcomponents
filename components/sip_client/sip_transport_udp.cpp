#include "sip_transport_udp.h"
#include <cstdio>
#include "esphome/core/log.h"
#include "sip_net.h"

namespace esphome {
namespace sip_client {

static const char *const TAG = "sip_client";

// Render an IPv4 sockaddr to dotted-quad without depending on inet_ntop.
static std::string sockaddr_ip(const struct sockaddr_storage &ss, uint16_t *port) {
  if (ss.ss_family != AF_INET) return "";
  const auto *s = reinterpret_cast<const struct sockaddr_in *>(&ss);
  uint32_t a = ntohl(s->sin_addr.s_addr);
  if (port != nullptr) *port = ntohs(s->sin_port);
  char buf[16];
  snprintf(buf, sizeof(buf), "%u.%u.%u.%u", static_cast<unsigned>((a >> 24) & 0xFF),
           static_cast<unsigned>((a >> 16) & 0xFF), static_cast<unsigned>((a >> 8) & 0xFF),
           static_cast<unsigned>(a & 0xFF));
  return std::string(buf);
}

bool UdpTransport::open(const std::string &server, uint16_t port) {
  struct sockaddr_storage server_addr;
  socklen_t sl = socket::set_sockaddr(reinterpret_cast<struct sockaddr *>(&server_addr),
                                      sizeof(server_addr), server.c_str(), port);
  if (sl == 0) {
    ESP_LOGW(TAG, "Invalid server address '%s' (use an IP)", server.c_str());
    return false;
  }
  // Create the socket to match the server address's actual family rather than
  // socket::socket_ip(), which is fixed to AF_INET6 whenever ESPHome's global
  // network::enable_ipv6 is on — that mismatched an IPv4 server (the common
  // case) against an IPv6-only socket and made connect() fail outright.
  this->socket_ = socket::socket(server_addr.ss_family, SOCK_DGRAM, IPPROTO_UDP);
  if (!this->socket_) {
    ESP_LOGW(TAG, "Could not create SIP socket");
    return false;
  }
  if (this->socket_->connect(reinterpret_cast<struct sockaddr *>(&server_addr), sl) != 0) {
    ESP_LOGW(TAG, "SIP connect failed");
    this->socket_.reset();
    return false;
  }
  this->socket_->setblocking(false);
  this->recv_buf_.resize(2048);

  // Learn our source IP/port (for Via/Contact/SDP) from the connected socket.
  struct sockaddr_storage local;
  socklen_t ll = sizeof(local);
  this->socket_->getsockname(reinterpret_cast<struct sockaddr *>(&local), &ll);
  this->local_ip_ = sockaddr_ip(local, &this->local_port_);
  if (this->local_ip_.empty() || this->local_ip_ == "0.0.0.0") {
    this->local_ip_ = local_interface_ipv4();
  }
  ESP_LOGI(TAG, "SIP socket bound, local %s:%u", this->local_ip_.c_str(), this->local_port_);
  return true;
}

void UdpTransport::close() {
  if (this->socket_) {
    this->socket_->close();
    this->socket_.reset();
  }
}

bool UdpTransport::send(const std::string &msg) {
  if (!this->socket_) return false;
  return this->socket_->write(msg.data(), msg.size()) >= 0;
}

bool UdpTransport::poll(std::string &out) {
  if (!this->socket_) return false;
  ssize_t len = this->socket_->read(this->recv_buf_.data(), this->recv_buf_.size() - 1);
  if (len <= 0) return false;
  out.assign(reinterpret_cast<char *>(this->recv_buf_.data()), len);
  return true;
}

std::string UdpTransport::via_sent_by() const {
  return this->local_ip_ + ":" + std::to_string(this->local_port_);
}

}  // namespace sip_client
}  // namespace esphome
