#pragma once

#include "uartex.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace uartex {

class TxTimeoutTrigger : public Trigger<>
{
public:
    explicit TxTimeoutTrigger(UARTExComponent *parent)
    {
        parent->add_on_error_callback([this](const ERROR error)
        {
            if (error == ERROR_TX_TIMEOUT) this->trigger();
        });
    }
};

class WriteTrigger : public Trigger<const uint8_t*, const uint16_t>
{
public:
    explicit WriteTrigger(UARTExComponent *parent)
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
    explicit ReadTrigger(UARTExComponent *parent)
    {
        parent->add_on_read_callback([this](const uint8_t *data, const uint16_t len)
        {
            this->trigger(data, len);
        });
    }
};

template <typename... Ts>
class UARTExWriteAction : public Action<Ts...>, public Parented<UARTExComponent>
{
public:
    void set_data_template(std::function<cmd_t(Ts...)> func)
    {
        this->data_func_ = func;
        this->static_ = false;
    }
    void set_data_static(const cmd_t &data)
    {
        this->data_static_ = data;
        this->static_ = true;
    }

    void play(Ts... x) override
    {
        if (this->static_)
        {
            this->parent_->enqueue_tx_data({nullptr, &this->data_static_});
        }
        else
        {
            data_static_ = this->data_func_(x...);
            this->parent_->enqueue_tx_data({nullptr, &this->data_static_});
        }
    }

protected:
    bool static_{false};
    std::function<cmd_t(Ts...)> data_func_{};
    cmd_t data_static_{};
};

}  // namespace uartex
}  // namespace esphome
