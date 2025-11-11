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
    if (this->tcp_port_ > 0) {
        this->server_ = socket::socket_ip(SOCK_STREAM, IPPROTO_TCP);
        if (!this->server_) {
            ESP_LOGW(TAG, "Could not create socket");
            return;
        }

        struct sockaddr_storage server_addr;
        socklen_t sl = socket::set_sockaddr_any(reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr), this->tcp_port_);

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
        ESP_LOGI(TAG, "Socket server started on port %d", this->tcp_port_);
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
            uint8_t buffer[256];
            ssize_t len = this->client_->read(buffer, sizeof(buffer));
            if (len > 0) {
                this->read_callback_.call(buffer, len);
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

bool TCP_ServerComponent::write(uint8_t *data, uint16_t len)
{
    if (this->client_) {
        this->client_->write(data, len);
        this->write_callback_.call(data, len);
        return true;
    }
    return false;
}


}  // namespace tcp_server
}  // namespace esphome