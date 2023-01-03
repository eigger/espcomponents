#include "botem_csm505.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace botem_csm505 {
static const char *TAG = "botem_csm505";
void BotemCSM505Component::dump_config()
{
    ESP_LOGCONFIG(TAG, "  RX Receive Timeout: %d", conf_rx_timeout_);
}

void BotemCSM505Component::setup()
{
    rx_parser_.add_headers({ 0x5E, 0x5B });
    rx_parser_.add_footers({ 0x5D, 0x0D });
    if (this->error_) this->error_->publish_state("None");
    if (this->version_) this->version_->publish_state(VERSION);
    this->publish_state(0);
    ESP_LOGI(TAG, "Initaialize.");
}

void BotemCSM505Component::loop()
{
    read_from_uart();
    publish_data();
}

void BotemCSM505Component::read_from_uart()
{
    rx_parser_.clear();
    bool valid_data = false;
    unsigned long timer = get_time();
    while (elapsed_time(timer) < conf_rx_timeout_)
    {
        while (!valid_data && this->available())
        {
            uint8_t byte;
            if (!this->read_byte(&byte)) continue;
            if (rx_parser_.parse_byte(byte)) valid_data = true;
            timer = get_time();
        }
        if (valid_data) break;
        delay(1);
    }
}

void BotemCSM505Component::publish_data()
{
    if (rx_parser_.buffer().size() == 0) return;
    if (validate_data() == false) return;
    if (rx_parser_.data().size() < 3) return;
    switch(rx_parser_.data()[2])
    {
    //In
    case 0x31:
        if (this->state < this->traits.get_max_value())
        {
            this->state += this->traits.get_step();
            this->publish_state(this->state);
        }
        break;
    //Out
    case 0x32:

        if (this->state > this->traits.get_min_value())
        {
            this->state -= this->traits.get_step();
            this->publish_state(this->state);
        }
        break;
    //Rx Error
    case 0x38:
        if (this->error_) this->error_->publish_state("Rx Error");
        break;
    //Tx Error
    case 0x39:
        if (this->error_) this->error_->publish_state("Tx Error");
        break;
    //Unknown
    default:
        break;
    }
}

void BotemCSM505Component::control(float value)
{
    if (this->state != value)
    {
        this->state = value;
        this->publish_state(value);
    }
}

unsigned long BotemCSM505Component::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long BotemCSM505Component::get_time()
{
    return millis();
}

bool BotemCSM505Component::validate_data()
{
    if (rx_parser_.data().size() == 0)
    {
        //ESP_LOGW(TAG, "[Read] Size error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        if (this->error_) this->error_->publish_state("Size Error");
        return false;
    }
    if (rx_parser_.parse_header() == false)
    {
        //ESP_LOGW(TAG, "[Read] Header error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        if (this->error_) this->error_->publish_state("Header Error");
        return false;
    }
    if (rx_parser_.parse_footer() == false)
    {
        //ESP_LOGW(TAG, "[Read] Footer error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        if (this->error_) this->error_->publish_state("Footer Error");
        return false;
    }
    if (this->error_) this->error_->publish_state("None");
    return true;
}

}  // namespace botem_csm505
}  // namespace esphome