#pragma once
#include "esphome/core/automation.h"
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace tcp_server {

enum TCP_MODE {
    TCP_MODE_READ_ONLY,
    TCP_MODE_READ_WRITE,
};


class TCP_ServerComponent : public Component
{
public:
    TCP_ServerComponent() = default;
    void set_tcp_port(uint16_t port);
    void set_tcp_mode(TCP_MODE mode);
    
    void add_on_write_callback(std::function<void(const uint8_t *data, const uint16_t len)> &&callback) { this->write_callback_.add(std::move(callback)); }
    void add_on_read_callback(std::function<void(const uint8_t *data, const uint16_t len)> &&callback) { this->read_callback_.add(std::move(callback)); }

    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::LATE; }
    
protected:
    
    CallbackManager<void(const uint8_t *data, const uint16_t len)> write_callback_{};
    CallbackManager<void(const uint8_t *data, const uint16_t len)> read_callback_{};
    uint16_t tcp_port_{0};
    TCP_MODE tcp_mode_{TCP_MODE_READ_WRITE};
    std::unique_ptr<socket::Socket> server_{nullptr};
    std::unique_ptr<socket::Socket> client_{nullptr};
};

} // namespace tcp_server
} // namespace esphome