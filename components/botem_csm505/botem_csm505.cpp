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
    if (this->count_) this->count_->publish_state(0);
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
    if (rx_parser_.data()[2] == '1')
    {
        if (this->state < this->traits.get_max_value())
        {
            this->state += this->traits.get_step();
            this->publish_state(this->state);
            if (this->count_) this->count_->publish_state(this->state);
        }
    }
    else if (rx_parser_.data()[2] == '2')
    {
        if (this->state > this->traits.get_min_value())
        {
            this->state -= this->traits.get_step();
            this->publish_state(this->state);
            if (this->count_) this->count_->publish_state(this->state);
        }
    }
}

void BotemCSM505Component::control(float value)
{
    if (this->state != value)
    {
        this->state = value;
        this->publish_state(value);
        if (this->count_) this->count_->publish_state(this->state);
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
    if (rx_parser_.data().size() != 3)
    {
        if (this->error_) this->error_->publish_state("Size Error");
        return false;
    }
    if (rx_parser_.parse_header() == false)
    {
        if (this->error_) this->error_->publish_state("Header Error");
        return false;
    }
    if (rx_parser_.parse_footer() == false)
    {
        if (this->error_) this->error_->publish_state("Footer Error");
        return false;
    }
    if (rx_parser_.data()[0] != '0' || rx_parser_.data()[1] != '0')
    {
        if (this->error_) this->error_->publish_state("Format Error");
        return false;
    }
    if (rx_parser_.data()[2] != '8')
    {
        if (this->error_) this->error_->publish_state("Rx Error");
        return false;
    }
    if (rx_parser_.data()[2] != '9')
    {
        if (this->error_) this->error_->publish_state("Tx Error");
        return false;
    }
    if (rx_parser_.data()[2] != '1' && rx_parser_.data()[2] != '2')
    {
        if (this->error_) this->error_->publish_state("Format Error");
        return false;
    }
    if (this->error_) this->error_->publish_state("None");
    return true;
}

}  // namespace botem_csm505
}  // namespace esphome