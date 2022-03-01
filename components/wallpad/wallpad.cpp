#include "wallpad.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace wallpad {
static const char *TAG = "wallpad";
void WallPadComponent::dump_config()
{
    ESP_LOGCONFIG(TAG, "  Baud Rate: %d", conf_baud_);
    ESP_LOGCONFIG(TAG, "  Data bits: %d", conf_data_);
    ESP_LOGCONFIG(TAG, "  Parity: %d", conf_parity_);
    ESP_LOGCONFIG(TAG, "  Stop bits: %d", conf_stop_);
    ESP_LOGCONFIG(TAG, "  RX Receive Timeout: %d", conf_rx_wait_);
    ESP_LOGCONFIG(TAG, "  TX Transmission Timeout: %d", conf_tx_wait_);
    ESP_LOGCONFIG(TAG, "  TX Retry Count: %d", conf_tx_retry_cnt_);
    LOG_PIN("  RX Pin: ", rx_pin_);
    LOG_PIN("  TX Pin: ", tx_pin_);
    if (ctrl_pin_) LOG_PIN("  Ctrl Pin: ", ctrl_pin_);
    if (rx_prefix_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_prefix: %s", hexencode(&rx_prefix_.value()[0], rx_prefix_len_).c_str());
    if (rx_suffix_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_suffix: %s", hexencode(&rx_suffix_.value()[0], rx_suffix_len_).c_str());
    if (tx_prefix_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_prefix: %s", hexencode(&tx_prefix_.value()[0], tx_prefix_len_).c_str());
    if (tx_suffix_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_suffix: %s", hexencode(&tx_suffix_.value()[0], tx_suffix_len_).c_str());
    ESP_LOGCONFIG(TAG, "  Data rx_checksum: %s", YESNO(rx_checksum_));
    ESP_LOGCONFIG(TAG, "  Data tx_checksum: %s", YESNO(tx_checksum_));
    if (state_response_.has_value()) ESP_LOGCONFIG(TAG, "  Data response: %s, offset: %d", hexencode(&state_response_.value().data[0], state_response_.value().data.size()).c_str(), state_response_.value().offset);
    ESP_LOGCONFIG(TAG, "  Device count: %d", devices_.size());
}

void WallPadComponent::setup()
{
#ifdef ARDUINO_ARCH_ESP8266
    unsigned char serialconfig = 0x10;
#endif
#ifdef ARDUINO_ARCH_ESP32
    uint32_t serialconfig = 0x8000010;
#endif

    serialconfig += conf_parity_;
    serialconfig += (conf_data_ - 5) << 2;
    if (conf_stop_ == 2) serialconfig += 0x20;
#ifdef ARDUINO_ARCH_ESP8266
    this->hw_serial_ = &Serial;
    this->hw_serial_->begin(conf_baud_, (SerialConfig)serialconfig);
#endif
#ifdef ARDUINO_ARCH_ESP32
    int8_t tx = this->tx_pin_ != nullptr ? this->tx_pin_->get_pin() : -1;
    int8_t rx = this->rx_pin_ != nullptr ? this->rx_pin_->get_pin() : -1;
    this->hw_serial_ = &Serial2;
    this->hw_serial_->begin(conf_baud_, serialconfig, rx, tx);
#endif
    if (this->ctrl_pin_)
    {
        this->ctrl_pin_->setup();
        this->ctrl_pin_->digital_write(RX_ENABLE);
    }

    if (rx_checksum_) this->rx_checksum_len_++;
    if (rx_checksum2_) this->rx_checksum_len_++;
    if (tx_checksum_) this->tx_checksum_len_++;
    if (tx_checksum2_) this->tx_checksum_len_++;

    ESP_LOGI(TAG, "HW Serial Initaialize.");
    rx_lastTime_ = set_time();
    tx_start_time_ = set_time();
    if (rx_prefix_.has_value()) parser_.add_headers(rx_prefix_.value());
    if (rx_suffix_.has_value()) parser_.add_footers(rx_suffix_.value());
}

void WallPadComponent::loop()
{
    // Receive Process
    rx_proc();

    // Publish Receive Packet
    publish_proc();
    
    // queue Process
    tx_proc();
}

void WallPadComponent::rx_proc()
{
    parser_.clear_buffer();
    rx_timeOut_ = conf_rx_wait_;
    while (rx_timeOut_ > 0)
    {
        while (this->hw_serial_->available())
        {
            if (parser_.parse_byte(this->hw_serial_->read())) return;
            if (validate() == ERR_NONE) return;
            rx_timeOut_ = conf_rx_wait_;  // if serial received, reset timeout counter
        }
        delay(1);
        rx_timeOut_--;
    }
}

void WallPadComponent::publish_proc()
{
     // Ack Timeout
    if (tx_ack_wait_ && elapsed_time(tx_start_time_) > conf_tx_wait_) tx_ack_wait_ = false;
    if (parser_.get_buffer().size() == 0) return;
    if (validate(true) != ERR_NONE) return;

    // for Ack
    if (tx_ack_wait_ && is_send_cmd())
    {
        auto *device = get_send_device();
        if (device->equal(parser_.get_data(), get_send_cmd()->ack, 0))
        {
            device->ack_ok();
            clear_send_cmd();
            ESP_LOGD(TAG, "Ack: %s, Gap Time: %lums", hexencode(parser_.get_buffer()).c_str(), elapsed_time(tx_start_time_));
            rx_lastTime_ = set_time();
            return;
        }
    }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "Receive data-> %s, Gap Time: %lums", hexencode(parser_.get_buffer()).c_str(), elapsed_time(rx_lastTime_));
#endif

    // Publish State
    bool found = false;
    for (auto *device : this->devices_)
    {
        if (device->parse_data(parser_.get_data()))
        {
            found = true;
        }
    }

#ifdef ESPHOME_LOG_HAS_VERBOSE
    if (!found)
    {
        ESP_LOGV(TAG, "Notfound data-> %s", hexencode(parser_.get_buffer()).c_str());
    }
#endif
    rx_lastTime_ = set_time();
}

void WallPadComponent::pop_tx_command()
{
    for (auto *device : this->devices_)
    {
        while(device->is_have_command())
        {
            const cmd_hex_t* cmd = device->pop_command();
            if (cmd == nullptr) continue;
            if (cmd->ack.size() == 0)   write_next_late({device, cmd});
            else                        write_next({device, cmd});
        }
    }
}
void WallPadComponent::tx_proc()
{
    if (parser_.get_buffer().size() > 0) return;
    if (elapsed_time(rx_lastTime_) < conf_tx_interval_) return;
    if (elapsed_time(tx_start_time_) < conf_tx_interval_) return;
    if (tx_ack_wait_) return;
    // Command retry
    if (is_send_cmd())
    {
        if (conf_tx_retry_cnt_ > tx_retry_cnt_)
        {
            ESP_LOGD(TAG, "Retry count: %d", tx_retry_cnt_);
            write_with_header(get_send_cmd()->data);
            tx_ack_wait_ = true;
            tx_retry_cnt_++;
            return;
        }
        else
        {
            get_send_device()->ack_ng();
            clear_send_cmd();
            ESP_LOGD(TAG, "Retry fail.");
        }
    }
    pop_tx_command();
    if (!tx_queue_.empty() || !tx_queue_late_.empty())
    {
        if (!tx_queue_.empty())
        {
            tx_send_cmd_ = tx_queue_.front();
            tx_queue_.pop();
        }
        else
        {
            tx_send_cmd_ = tx_queue_late_.front();
            tx_queue_late_.pop();
        }
        write_with_header(tx_send_cmd_.cmd->data);
        if (tx_send_cmd_.cmd->ack.size() > 0)
        {
            tx_ack_wait_ = true;
            tx_retry_cnt_ = 1;
        }
        else
        {
            get_send_device()->ack_ok();
            clear_send_cmd();
        }
    }
}

void WallPadComponent::write_with_header(const std::vector<uint8_t> &data)
{
    tx_start_time_ = set_time();
    if (ctrl_pin_) ctrl_pin_->digital_write(TX_ENABLE);
    if (true)
    {
        std::vector<uint8_t> buffer;
        if (tx_prefix_.has_value()) buffer.insert(buffer.end(), tx_prefix_.value().begin(), tx_prefix_.value().end());
        buffer.insert(buffer.end(), data.begin(), data.end());
        uint8_t crc = 0;
        if (tx_checksum_)
        {
            crc = make_tx_checksum(&(data[0]), data.size());
            buffer.push_back(crc);
        }

        if (tx_checksum2_)
        {
            crc = make_tx_checksum2(&(data[0]), data.size(), crc);
            buffer.push_back(crc);
        }
        if (tx_suffix_.has_value()) buffer.insert(buffer.end(), tx_suffix_.value().begin(), tx_suffix_.value().end());
        write_array(buffer);
    }
    else
    {
        // Header
        if (tx_prefix_.has_value()) write_array(tx_prefix_.value());

        // Data part
        write_array(data);

        // XOR Checksum
        uint8_t crc = 0;
        if (tx_checksum_) write_byte(crc = make_tx_checksum(&(data[0]), data.size()));

        // ADD Checksum
        if (tx_checksum2_) write_byte(make_tx_checksum2(&(data[0]), data.size(), crc));

        // Footer
        if (tx_suffix_.has_value()) write_array(tx_suffix_.value());
    }
    // wait for send
    if (ctrl_pin_)
    {
        flush();
        ctrl_pin_->digital_write(RX_ENABLE);
    }

    // for Ack wait
    tx_start_time_ = set_time();
}

void WallPadComponent::write_byte(uint8_t data)
{
    this->hw_serial_->write(data);
    ESP_LOGD(TAG, "Write byte-> 0x%02X", data);
}

void WallPadComponent::write_array(const uint8_t *data, const num_t len)
{
    this->hw_serial_->write(data, len);
    ESP_LOGD(TAG, "Write array-> %s", hexencode(&data[0], len).c_str());
}

void WallPadComponent::write_next(const send_hex_t send)
{
    tx_queue_.push(send);
}

void WallPadComponent::write_next_late(const send_hex_t send)
{
    tx_queue_late_.push(send);
}

void WallPadComponent::flush()
{
    this->hw_serial_->flush(true);
    ESP_LOGD(TAG, "Flushing... (%lums)", elapsed_time(tx_start_time_));
}

ValidateCode WallPadComponent::validate(bool log)
{
    if (parser_.get_data().size() < rx_checksum_len_)
    {
        if (log) ESP_LOGW(TAG, "[Read] Size error: %s", hexencode(parser_.get_buffer()).c_str());
        return ERR_SIZE;
    }
    if (rx_prefix_.has_value() && parser_.parse_header() == false)
    {
        if (log) ESP_LOGW(TAG, "[Read] Prefix error: %s", hexencode(parser_.get_buffer()).c_str());
        return ERR_PREFIX;
    }
    if (rx_suffix_.has_value() && parser_.parse_footer() == false)
    {
        if (log) ESP_LOGW(TAG, "[Read] Suffix error: %s", hexencode(parser_.get_buffer()).c_str());
        return ERR_SUFFIX;
    }
    uint8_t crc = rx_checksum_ ? make_rx_checksum(&parser_.get_data()[0], parser_.get_data().size() - rx_checksum_len_) : 0;
    if (rx_checksum_ && crc != parser_.get_data()[parser_.get_data().size() - rx_checksum_len_])
    {
        if (log) ESP_LOGW(TAG, "[Read] Checksum error: %s", hexencode(parser_.get_buffer()).c_str());
        return ERR_CHECKSUM;
    }
    if (rx_checksum2_ && make_rx_checksum2(&parser_.get_data()[0], parser_.get_data().size() - rx_checksum_len_, crc) != parser_.get_data()[parser_.get_data().size() - rx_checksum_len_ - 1])
    {
        if (log) ESP_LOGW(TAG, "[Read] Checksum2 error: %s", hexencode(parser_.get_buffer()).c_str());
        return ERR_CHECKSUM2;
    }
    return ERR_NONE;
}

uint8_t WallPadComponent::make_rx_checksum(const uint8_t *data, const num_t len) const
{
    if (this->rx_checksum_f_.has_value())
    {
        return (*rx_checksum_f_)(data, len);
    }
    else
    {
        // CheckSum8 Xor
        uint8_t crc = 0;
        if (this->rx_prefix_.has_value())
        {
            for (num_t i = 0; i < this->rx_prefix_len_; i++)
            {
                crc ^= this->rx_prefix_.value()[i];
            }
        }
        for (num_t i = 0; i < len; i++)
        {
            crc ^= data[i];
        }
        return crc;
    }
}

uint8_t WallPadComponent::make_rx_checksum2(const uint8_t *data, const num_t len, const uint8_t checksum1) const
{
    if (this->rx_checksum2_f_.has_value())
    {
        return (*rx_checksum2_f_)(data, len, checksum1);
    }
    else
    {
        // CheckSum8 Add
        uint8_t crc = 0;
        if (this->rx_prefix_.has_value())
        {
            for (num_t i = 0; i < this->rx_prefix_len_; i++)
            {
                crc += this->rx_prefix_.value()[i];
            }
        }

        for (num_t i = 0; i < len; i++)
        {
            crc += data[i];
        }
            
        if (this->rx_checksum_) crc += checksum1;
        return crc;
    }
}

uint8_t WallPadComponent::make_tx_checksum(const uint8_t *data, const num_t len) const
{
    if (this->tx_checksum_f_.has_value())
    {
        return (*tx_checksum_f_)(data, len);
    }
    else
    {
        // CheckSum8 Xor
        uint8_t crc = 0;
        if (this->tx_prefix_.has_value())
        {
            for (num_t i = 0; i < this->tx_prefix_len_; i++)
            {
                crc ^= this->tx_prefix_.value()[i];
            }
        }
        for (num_t i = 0; i < len; i++)
        {
            crc ^= data[i];
        }
        return crc;
    }
}

uint8_t WallPadComponent::make_tx_checksum2(const uint8_t *data, const num_t len, const uint8_t checksum1) const
{
    if (this->tx_checksum2_f_.has_value())
    {
        return (*tx_checksum2_f_)(data, len, checksum1);
    }
    else
    {
        // CheckSum8 Add
        uint8_t crc = 0;
        if (this->tx_prefix_.has_value())
        {
            for (num_t i = 0; i < this->tx_prefix_len_; i++)
            {
                crc += this->tx_prefix_.value()[i];
            }
        }

        for (num_t i = 0; i < len; i++)
        {
            crc += data[i];
        }
            
        if (this->tx_checksum_) crc += checksum1;
        return crc;
    }
}

}  // namespace wallpad
}  // namespace esphome