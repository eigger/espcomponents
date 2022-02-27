#include "wallpad_device.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace wallpad {
static const char *TAG = "wallpad";
void WallPadDevice::update()
{
    if (!command_state_.has_value()) return;
    ESP_LOGD(TAG, "'%s' update(): Request current state...", device_name_->c_str());
    push_command(&command_state_.value());
}

void WallPadDevice::dump_wallpad_device_config(const char *TAG)
{
    ESP_LOGCONFIG(TAG, "  Device: %s, offset: %d", hexencode(&device_.data[0], device_.data.size()).c_str(), device_.offset);
    if (sub_device_.has_value()) ESP_LOGCONFIG(TAG, "  Sub device: %s, offset: %d", hexencode(&sub_device_.value().data[0], sub_device_.value().data.size()).c_str(), sub_device_.value().offset);
    if (state_on_.has_value()) ESP_LOGCONFIG(TAG, "  State ON: %s, offset: %d, and_operator: %s, inverted: %s", hexencode(&state_on_.value().data[0], state_on_.value().data.size()).c_str(), state_on_.value().offset, YESNO(state_on_.value().and_operator), YESNO(state_on_.value().inverted));
    if (state_off_.has_value()) ESP_LOGCONFIG(TAG, "  State OFF: %s, offset: %d, and_operator: %s, inverted: %s", hexencode(&state_off_.value().data[0], state_off_.value().data.size()).c_str(), state_off_.value().offset, YESNO(state_off_.value().and_operator), YESNO(state_off_.value().inverted));
    if (command_on_.has_value()) ESP_LOGCONFIG(TAG, "  Command ON: %s", hexencode(&command_on_.value().data[0], command_on_.value().data.size()).c_str());
    if (command_on_.has_value() && command_on_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command ON Ack: %s", hexencode(&command_on_.value().ack[0], command_on_.value().ack.size()).c_str());
    if (command_off_.has_value()) ESP_LOGCONFIG(TAG, "  Command OFF: %s", hexencode(&command_off_.value().data[0], command_off_.value().data.size()).c_str());
    if (command_off_.has_value() && command_off_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command OFF Ack: %s", hexencode(&command_off_.value().ack[0], command_off_.value().ack.size()).c_str());
    if (command_state_.has_value()) ESP_LOGCONFIG(TAG, "  Command State: %s", hexencode(&command_state_.value().data[0], command_state_.value().data.size()).c_str());
    if (command_state_.has_value() && command_state_.value().ack.size() > 0) ESP_LOGCONFIG(TAG, "  Command State Ack: %s", hexencode(&command_state_.value().ack[0], command_state_.value().ack.size()).c_str());
    LOG_UPDATE_INTERVAL(this);
}

bool WallPadDevice::parse_data(const uint8_t *data, const num_t len)
{
    if (tx_pending_) return false;

    if (!compare(&data[0], len, &device_)) return false;
    else if (sub_device_.has_value() && !compare(&data[0], len, &sub_device_.value())) return false;

    // Turn OFF Message
    if (state_off_.has_value() && compare(&data[0], len, &state_off_.value()))
    {
        if (!publish(false)) publish(data, len);
        return true;
    }
    // Turn ON Message
    else if (state_on_.has_value() && compare(&data[0], len, &state_on_.value()))
    {
        if (!publish(true)) publish(data, len);
        return true;
    }

    // Other Message
    publish(data, len);
    return true;
}

void WallPadDevice::push_command(const cmd_hex_t *cmd)
{
    set_tx_pending(true);
    tx_cmd_queue_.push(cmd);
}

std::string hexencode(const uint8_t *raw_data, num_t len)
{
    char buf[20];
    std::string res;
    for (num_t i = 0; i < len; i++)
    {
        sprintf(buf, "0x%02X ", raw_data[i]);
        res += buf;
    }
    sprintf(buf, "(%d byte)", len);
    res += buf;
    return res;
}

bool compare(const uint8_t *data1, const num_t len1, const uint8_t *data2, const num_t len2, const num_t offset)
{
    if (len1 - offset < len2) return false;
    //ESP_LOGD(TAG, "compare(0x%02X, 0x%02X, %d)=> %d", data1[offset], data2[0], len2, memcmp(&data1[offset], &data2[0], len2));
    return memcmp(&data1[offset], &data2[0], len2) == 0;
}

bool compare(const uint8_t *data1, const num_t len1, const hex_t *data2)
{
    if (!data2->and_operator) return compare(data1, len1, &data2->data[0], data2->data.size(), data2->offset) ? !data2->inverted : data2->inverted;
    else if (len1 - data2->offset > 0 && data2->data.size() > 0)
    {
        uint8_t val = data1[data2->offset] & (data2->data[0]);
        if (data2->data.size() == 1) return val ? !data2->inverted : data2->inverted;
        else
        {
            bool ret = false;
            for (num_t i = 1; i < data2->data.size(); i++)
            {
                if (val == data2->data[i])
                {
                    ret = true;
                    break;
                }
            }
            return ret ? !data2->inverted : data2->inverted;
        }
    }
    else return false;
}

float hex_to_float(const uint8_t *data, const num_t len, const num_t precision)
{
    unsigned int val = 0;
    for (num_t i = 0; i < len; i++)
    {
        val = (val << 8) | data[i];
    }
    return val / powf(10, precision);
}

unsigned long elapsed_time(const unsigned long timer)
{
    return millis() - timer; 
}
unsigned long set_time()
{
    return millis();
}

}  // namespace wallpad
}  // namespace esphome