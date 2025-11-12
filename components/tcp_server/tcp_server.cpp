#include "tcp_server.h"

namespace esphome {
namespace tcp_server {
static const char *TAG = "tcp_server";
void TCP_ServerComponent::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG

#endif
}

void TCP_ServerComponent::setup()
{
    recv_buffer_.resize(recv_buffer_size_);
    if (this->port_ > 0) {
        this->server_ = socket::socket_ip(SOCK_STREAM, IPPROTO_TCP);
        if (!this->server_) {
            ESP_LOGW(TAG, "Could not create socket");
            return;
        }

        struct sockaddr_storage server_addr;
        socklen_t sl = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr), this->port_);

        if (this->server_->bind(reinterpret_cast<struct sockaddr *>(&server_addr), sl) != 0) {
            ESP_LOGW(TAG, "bind failed");
            this->server_.reset();
            return;
        }
        if (this->server_->listen(1) != 0) {
            ESP_LOGW(TAG, "listen failed");
            this->server_.reset();
            return;
        }
        if (this->server_->setblocking(false) != 0) {
            ESP_LOGW(TAG, "setblocking failed");
            this->server_.reset();
            return;
        }
        ESP_LOGI(TAG, "Socket server started on port %d", this->port_);
    }
    ESP_LOGI(TAG, "Initaialize");

}

void TCP_ServerComponent::loop()
{
    if (this->server_) {
        if (this->client_ == nullptr) {
            struct sockaddr_storage client_addr;
            socklen_t sl = sizeof(client_addr);
            this->client_ = this->server_->accept(reinterpret_cast<struct sockaddr *>(&client_addr), &sl);
            if (this->client_) {
                ESP_LOGI(TAG, "New client connected");
                this->client_->setblocking(false);
            }
        } else {
            ssize_t len = this->client_->read(&recv_buffer_[0], recv_buffer_.size());
            if (len > 0) {
                this->read_callback_.call(&recv_buffer_[0], len);
            } else if (len == 0) {
                ESP_LOGI(TAG, "Client disconnected");
                this->client_.reset();
            } else if (errno != EAGAIN) {
                ESP_LOGW(TAG, "Socket read error: %s", strerror(errno));
                this->client_.reset();
            }
        }
    }
}

bool TCP_ServerComponent::write_array(const uint8_t* data, size_t len) 
{
    if (!this->client_ || data == nullptr || len == 0) return false;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = this->client_->write(data + sent, len - sent);
        if (n > 0) {
            sent += static_cast<size_t>(n);
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else {
            ESP_LOGW(TAG, "Socket write error: %s", strerror(errno));
            break;
        }
    }

    if (sent > 0) {
        this->write_callback_.call(data, sent);
    }
    return sent == len;
}

bool TCP_ServerComponent::write_array(const std::vector<uint8_t>& data) 
{
    return this->write_array(data.data(), data.size());
}

bool TCP_ServerComponent::write_array(std::string_view s) 
{
    return this->write_array(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

bool TCP_ServerComponent::write_array(uint8_t* data, uint16_t len) 
{
    return this->write_array(static_cast<const uint8_t*>(data), static_cast<size_t>(len));
}

}  // namespace tcp_server
}  // namespace esphome