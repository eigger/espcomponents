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
    init_ = true;
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
    memset(&rx_buffer_, 0, BUFFER_SIZE);
    rx_timeOut_ = conf_rx_wait_;
    rx_bytesRead_ = 0;
    num_t offset = 0;
    num_t length = 0;
    uint8_t buffer_byte = 0;
    while (rx_timeOut_ > 0)
    {
        while (this->hw_serial_->available())
        {
            buffer_byte = this->hw_serial_->read();
            if (rx_bytesRead_ < BUFFER_SIZE)
            {
                rx_buffer_[rx_bytesRead_++] = buffer_byte;
                if (rx_prefix_.has_value())
                {
                    length = rx_prefix_len_ > rx_bytesRead_ ? rx_bytesRead_ : rx_prefix_len_;
                    if (!compare(&rx_buffer_[0], rx_bytesRead_, &rx_prefix_.value()[0], length, 0))
                    {
                        rx_bytesRead_ = 0;
                    }
                }
                if (rx_suffix_.has_value() && rx_bytesRead_ > rx_prefix_len_ + rx_suffix_len_)
                {
                    offset = rx_bytesRead_ - rx_suffix_len_;
                    if (compare(&rx_buffer_[0], rx_bytesRead_, &rx_suffix_.value()[0], rx_suffix_len_, offset))
                    {
                        return;
                    }
                }
                if (!rx_suffix_.has_value())
                {
                    length = 0;
                    if (rx_prefix_.has_value()) length += rx_prefix_len_;
                    if (rx_checksum_ || rx_checksum2_) length += rx_checksum_len_;
                    if (rx_bytesRead_ >= length && validate(&rx_buffer_[0], rx_bytesRead_) == ERR_NONE)
                    {
                        return;
                    }
                }
            }
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
    if (rx_bytesRead_ == 0) return;

    rx_buffer_[rx_bytesRead_] = 0; // before logging as a char array, zero terminate the last position to be safe.

    ValidateCode code = validate(&rx_buffer_[0], rx_bytesRead_);
    log_errcode(code, &rx_buffer_[0], rx_bytesRead_);
    if (code != ERR_NONE) return;

    // Patket type
    if (state_response_.has_value())
    {
        if (compare(&rx_buffer_[rx_prefix_len_], rx_bytesRead_ - rx_prefix_len_, &state_response_.value()))
        {
            response_wait_ = false;
        }
        else
        {
            response_wait_ = true;
        }
            
    }

    // for Ack
    if (tx_ack_wait_ && tx_current_cmd_)
    {
        if (compare(&rx_buffer_[rx_prefix_len_], rx_bytesRead_ - rx_prefix_len_, &tx_current_cmd_->ack[0], tx_current_cmd_->ack.size(), 0))
        {
            tx_current_cmd_ = nullptr;
            tx_ack_wait_ = false;
            tx_retry_cnt_ = 0;

            if (tx_current_device_)
            {
                tx_current_device_->set_tx_pending(false);
                tx_current_device_ = nullptr;
            }
            ESP_LOGD(TAG, "Ack: %s, Gap Time: %lums", hexencode(rx_buffer_, rx_bytesRead_).c_str(), elapsed_time(tx_start_time_));
            rx_lastTime_ = set_time();
            return;
        }
    }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "Receive data-> %s, Gap Time: %lums", hexencode(&rx_buffer_[0], rx_bytesRead_).c_str(), elapsed_time(rx_lastTime_));
#endif

    // Publish State
    bool found = false;
    for (auto *device : this->devices_)
    {
        if (device->parse_data(&rx_buffer_[rx_prefix_len_], rx_bytesRead_ - rx_prefix_len_ - rx_suffix_len_))
        {
            found = true;
        }
    }


#ifdef ESPHOME_LOG_HAS_VERBOSE
    if (!found)
    {
        ESP_LOGV(TAG, "Notfound data-> %s", hexencode(&rx_buffer_[0], rx_bytesRead_).c_str());
    }
#endif
    rx_lastTime_ = set_time();
}

void WallPadComponent::tx_proc()
{
    if (rx_bytesRead_ > 0) return;
    if (response_wait_) return;
    if (elapsed_time(rx_lastTime_) < conf_tx_interval_) return;
    if (elapsed_time(tx_start_time_) < conf_tx_interval_) return;
    // Command retry
    if (!tx_ack_wait_ && tx_current_cmd_)
    {
        if (conf_tx_retry_cnt_ > tx_retry_cnt_)
        {
            ESP_LOGD(TAG, "Retry count: %d", tx_retry_cnt_);
            write_with_header(tx_current_cmd_->data);
            tx_ack_wait_ = true;
            tx_retry_cnt_++;
            return;
        }
        else
        {
            tx_current_cmd_ = nullptr;
            tx_ack_wait_ = false;
            tx_retry_cnt_ = 0;

            if (tx_current_device_)
            {
                tx_current_device_->set_tx_pending(false);
                tx_current_device_ = nullptr;
            }
            ESP_LOGD(TAG, "Retry fail.");
        }
    }

    // for State request
    if (tx_queue_.empty() && !tx_queue_late_.empty())
    {
        write_with_header(tx_queue_late_.front()->data);
        if (tx_queue_late_.front()->ack.size() > 0)
        {
            tx_current_cmd_ = tx_queue_late_.front();
            tx_current_device_ = nullptr;
            tx_ack_wait_ = true;
            tx_retry_cnt_ = 1;
        }
        else
        {
            tx_current_cmd_ = nullptr;
            tx_current_device_ = nullptr;
            tx_ack_wait_ = true;
            tx_retry_cnt_ = 1;
        }
        tx_queue_late_.pop();
    }

    // for Command
    else if (!tx_queue_.empty())
    {
        write_with_header(tx_queue_.front().cmd->data);
        // Pending Ack
        if (tx_queue_.front().cmd->ack.size() > 0)
        {
            tx_current_cmd_ = tx_queue_.front().cmd;
            tx_current_device_ = tx_queue_.front().device;
            tx_ack_wait_ = true;
            tx_retry_cnt_ = 1;
        }
        else if (tx_queue_.front().device)
        {
            (tx_queue_.front().device)->set_tx_pending(false);
            tx_ack_wait_ = true;
        }
        else
        {
            tx_ack_wait_ = true;
        }
        tx_queue_.pop();
    }
}

void WallPadComponent::write_with_header(const std::vector<uint8_t> &data)
{
    tx_start_time_ = set_time();
    if (ctrl_pin_) ctrl_pin_->digital_write(TX_ENABLE);
    if (false)
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
    flush();
    if (ctrl_pin_) ctrl_pin_->digital_write(RX_ENABLE);

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
    ESP_LOGD(TAG, "Write array-> %s, %d", hexencode(&data[0], len).c_str(), this->hw_serial_->availableForWrite());
}

void WallPadComponent::write_next(const send_hex_t send)
{
    if (!init_)
    {
        if (send.device) (send.device)->set_tx_pending(false);
        return;
    }
    tx_queue_.push(send);
}

void WallPadComponent::write_next_late(const cmd_hex_t *cmd)
{
    if (!init_) return;
    tx_queue_late_.push(cmd);
}

void WallPadComponent::flush()
{
    this->hw_serial_->flush(true);
    ESP_LOGD(TAG, "Flushing... (%lums)", elapsed_time(tx_start_time_));
}

ValidateCode WallPadComponent::validate(const uint8_t *data, const num_t len)
{
    if (rx_prefix_.has_value() && !compare(&data[0], len, &rx_prefix_.value()[0], rx_prefix_len_, 0))
    {
        //ESP_LOGW(TAG, "[Read] Prefix not match: %s", hexencode(&data[0], len).c_str());
        return ERR_PREFIX;
    }
    if (rx_suffix_.has_value() && !compare(&data[0], len, &rx_suffix_.value()[0], rx_suffix_len_, len - rx_suffix_len_))
    {
        //ESP_LOGW(TAG, "[Read] Suffix not match: %s", hexencode(&data[0], len).c_str());
        return ERR_SUFFIX;
    }
    uint8_t crc = rx_checksum_ ? make_rx_checksum(&data[rx_prefix_len_], len - rx_prefix_len_ - rx_suffix_len_ - rx_checksum_len_) : 0;
    if (rx_checksum_ && crc != data[len - rx_suffix_len_ - rx_checksum_len_])
    {
        //ESP_LOGW(TAG, "[Read] Checksum error: %s", hexencode(&data[0], len).c_str());
        return ERR_CHECKSUM;
    }
    if (rx_checksum2_ && make_rx_checksum2(&data[rx_prefix_len_], len - rx_prefix_len_ - rx_suffix_len_ - rx_checksum_len_, crc) != data[len - rx_suffix_len_ - 1])
    {
        //ESP_LOGW(TAG, "[Read] Checksum2 error: %s", hexencode(&data[0], len).c_str());
        return ERR_CHECKSUM2;
    }
    return ERR_NONE;
}

void WallPadComponent::log_errcode(ValidateCode code, const uint8_t *data, const num_t len)
{
    switch(code)
    {
    case ERR_NONE:
        break;
    case ERR_PREFIX:
        ESP_LOGW(TAG, "[Read] Prefix not match: %s", hexencode(&data[0], len).c_str());
        break;
    case ERR_SUFFIX:
        ESP_LOGW(TAG, "[Read] Suffix not match: %s", hexencode(&data[0], len).c_str());
        break;
    case ERR_CHECKSUM:
        ESP_LOGW(TAG, "[Read] Checksum error: %s", hexencode(&data[0], len).c_str());
        break;
    case ERR_CHECKSUM2:
        ESP_LOGW(TAG, "[Read] Checksum2 error: %s", hexencode(&data[0], len).c_str());
        break;
    }
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

void WallPadDevice::update()
{
    if (!command_state_.has_value()) return;
    ESP_LOGD(TAG, "'%s' update(): Request current state...", device_name_->c_str());
    write_next_late(&command_state_.value());
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

void WallPadDevice::write_with_header(const cmd_hex_t *cmd)
{
    set_tx_pending(true);
    write_next({this, cmd});
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