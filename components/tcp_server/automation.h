#pragma once

#include "tcp_server.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace tcp_server {

class WriteTrigger : public Trigger<const uint8_t*, const uint16_t>
{
public:
    explicit WriteTrigger(TCP_ServerComponent *parent)
    {
        parent->add_on_write_callback([this](const uint8_t *data, const uint16_t len)
        {
            this->trigger(data, len);
        });
    }
};

class ReadTrigger : public Trigger<const uint8_t*, const uint16_t>
{
public:
    explicit ReadTrigger(TCP_ServerComponent *parent)
    {
        parent->add_on_read_callback([this](const uint8_t *data, const uint16_t len)
        {
            this->trigger(data, len);
        });
    }
};

template <typename... Ts>
class TCP_ServerWriteAction : public Action<Ts...>, public Parented<TCP_ServerComponent>
{
public:
    void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func)
    {
        this->data_func_ = func;
        this->static_ = false;
    }
    void set_data_static(std::vector<uint8_t>& data)
    {
        this->data_static_ = data;
        this->static_ = true;
    }

    void play(Ts... x) override
    {
        if (this->static_)
        {
            this->parent_->write_array(this->data_static_);
        }
        else
        {
            data_static_ = this->data_func_(x...);
            this->parent_->write_array(this->data_static_);
        }
    }

protected:
    bool static_{false};
    std::function<std::vector<uint8_t>(Ts...)> data_func_{};
    std::vector<uint8_t> data_static_{};
};

}  // namespace tcp_server
}  // namespace esphome
