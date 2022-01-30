#pragma once
#include "define.h"
#include "wallpad_component.h"

/**
 * WallPad Listener
 * @desc 각 컴포넌트에 수신 메시지 전달
 */
namespace esphome {
namespace wallpad {
class WallPadListener
{
public:
    virtual bool parse_data(const uint8_t *data, const num_t len) = 0;
    void set_parent(WallPadComponent *parent) { parent_ = parent; }

    void set_monitor(bool monitor) { monitor_ = monitor; }
    bool is_monitor() { return monitor_; }

protected:
    WallPadComponent *parent_{nullptr};
    bool monitor_{false};
};
}
}