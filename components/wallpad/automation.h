#pragma once

#include "wallpad_component.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace wallpad {

template <typename... Ts>
class WallPadWriteAction : public Action<Ts...>, public Parented<WallPadComponent>
{
public:
    void set_data_template(std::function<cmd_hex_t(Ts...)> func)
    {
        this->data_func_ = func;
        this->static_ = false;
    }
    void set_data_static(const cmd_hex_t &data)
    {
        this->data_static_ = data;
        this->static_ = true;
    }

    void play(Ts... x) override
    {
        if (this->static_)
        {
            this->parent_->write_next({nullptr, &this->data_static_});
        }
        else
        {
            data_static_ = this->data_func_(x...);
            this->parent_->write_next({nullptr, &this->data_static_});
        }
    }

protected:
    bool static_{false};
    std::function<cmd_hex_t(Ts...)> data_func_{};
    cmd_hex_t data_static_{};
};

}  // namespace wallpad
}  // namespace esphome
