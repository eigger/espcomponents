#pragma once
#include "common.h"
#include "wallpad_listener.h"

namespace esphome {
namespace wallpad {
/** 패킷 캡쳐용 리스너 */
class SerialMonitor : public WallPadListener
{
public:
    SerialMonitor(hex_t filter = {})
    {
        if (!filter.data.empty()) filters_.push_back(filter);
        set_monitor(true);
    }
    void add_filter(hex_t filter) { filters_.push_back(filter); }
    bool parse_data(const uint8_t *data, const num_t len) override;

protected:
    std::vector<hex_t> filters_;
};

}
}