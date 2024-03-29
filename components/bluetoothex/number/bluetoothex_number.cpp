#include "bluetoothex_number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace bluetoothex {

static const char *TAG = "bluetoothex.number";

void BluetoothExNumber::dump_config()
{
    ESP_LOGCONFIG(TAG, "BluetoothEx Number '%s':", device_name_->c_str());
    dump_bluetoothex_device_config(TAG);
}

void BluetoothExNumber::setup()
{
    this->state = this->traits.get_min_value();
    this->publish_state(this->state);
}

void BluetoothExNumber::publish(const std::vector<uint8_t>& data)
{
    if (this->state_number_func_.has_value())
    {
        optional<float> val = (*this->state_number_func_)(&data[0], data.size(), this->state);
        if (val.has_value() && this->state != val.value())
        {
            this->state = val.value();
            this->publish_state(this->state);
        }
    }
    else if (this->state_number_.has_value() && data.size() >= (this->state_number_.value().offset + this->state_number_.value().length))
    {
        float val = state_to_float(data, this->state_number_.value());
        if (this->state != val)
        {
            this->state = val;
            this->publish_state(this->state);
        }
    }
}

void BluetoothExNumber::control(float value)
{
    if (this->command_number_func_.has_value())
    {
        if (this->state != value)
        {
            command_number_ = (*this->command_number_func_)(value);
            push_tx_cmd(&command_number_);
        }
    }
    this->state = value;
    this->publish_state(value);
}

}  // namespace bluetoothex
}  // namespace esphome
