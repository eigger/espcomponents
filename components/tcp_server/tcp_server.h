#pragma once
#include "esphome/core/automation.h"
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace tcp_server {

class TCP_ServerComponent : public Component
{
public:
    TCP_ServerComponent() = default;
    void set_port(uint16_t port) { this->port_ = port; }
    void set_recv_buffer_size(size_t size) { this->recv_buffer_size_ = size; }
    void add_on_write_callback(std::function<void(const uint8_t *data, const uint16_t len)> &&callback) { this->write_callback_.add(std::move(callback)); }
    void add_on_read_callback(std::function<void(const uint8_t *data, const uint16_t len)> &&callback) { this->read_callback_.add(std::move(callback)); }

    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::LATE; }
    bool write_array(const uint8_t* data, size_t len);
    bool write_array(const std::vector<uint8_t>& data);
    bool write_array(std::string_view s);
    bool write_array(uint8_t* data, uint16_t len);
protected:
    
    CallbackManager<void(const uint8_t *data, const uint16_t len)> write_callback_{};
    CallbackManager<void(const uint8_t *data, const uint16_t len)> read_callback_{};
    uint16_t port_{0};
    size_t recv_buffer_size_{256};
    std::vector<uint8_t> recv_buffer_{};
    std::unique_ptr<socket::Socket> server_{nullptr};
    std::unique_ptr<socket::Socket> client_{nullptr};
};

} // namespace tcp_server
} // namespace esphome