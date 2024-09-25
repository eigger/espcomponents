#include "botem_csm505.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace botem_csm505 {
static const char *TAG = "botem_csm505.number";

void BotemCSM505::dump_config()
{
    ESP_LOGCONFIG(TAG, "BotemCSM505 '%s':", get_name().c_str());
    uartex_dump_config(TAG);
}

void BotemCSM505::setup()
{
    this->state = this->traits.get_min_value();
    publish_state(this->state);
}

void BotemCSM505::publish(const std::vector<uint8_t>& data)
{
    if (data.size() != 3) return;
    if (uartex::verify_state(data, &entry_state_))
    {
        if (this->state < this->traits.get_max_value())
        {
            this->state += this->traits.get_step();
            this->publish_state(this->state);
        }
    }
    else if (uartex::verify_state(data, &exit_state_))
    {
        if (this->state > this->traits.get_min_value())
        {
            this->state -= this->traits.get_step();
            this->publish_state(this->state);
        }
    }
}

void BotemCSM505::control(float value)
{
    if (this->state == value) return;
    this->state = value;
    publish_state(value);
}
}  // namespace botem_csm505
}  // namespace esphome