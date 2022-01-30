#include "serial_monitor.h"

namespace esphome {
namespace wallpad {


bool SerialMonitor::parse_data(const uint8_t *data, const num_t len)
{
    bool found = false;
    if (filters_.size() == 0) found = true;
    else
    {
        for (hex_t filter : filters_)
        {
            found = compare(&data[0], len, &filter);
            if (found) break;
        }
    }
    if (found) ESP_LOGI(TAG, "Serial Monitor: %s", hexencode(&data[0], len).c_str());
    return found;
}


}
}