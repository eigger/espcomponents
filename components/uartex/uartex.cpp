#include "uartex.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uartex {
static const char *TAG = "uartex";
void UARTExComponent::dump_config()
{
    ESP_LOGCONFIG(TAG, "  RX Receive Timeout: %d", this->conf_rx_timeout_);
    ESP_LOGCONFIG(TAG, "  TX Transmission Timeout: %d", this->conf_tx_timeout_);
    ESP_LOGCONFIG(TAG, "  TX Retry Count: %d", this->conf_tx_retry_cnt_);
    if (this->tx_ctrl_pin_)   LOG_PIN("  TX Ctrl Pin: ", this->tx_ctrl_pin_);
    if (this->rx_header_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_header: %s", to_hex_string(this->rx_header_.value()).c_str());
    if (this->rx_footer_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_footer: %s", to_hex_string(this->rx_footer_.value()).c_str());
    if (this->tx_header_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_header: %s", to_hex_string(this->tx_header_.value()).c_str());
    if (this->tx_footer_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_footer: %s", to_hex_string(this->tx_footer_.value()).c_str());
    ESP_LOGCONFIG(TAG, "  Data rx_checksum: %d", this->rx_checksum_);
    ESP_LOGCONFIG(TAG, "  Data tx_checksum: %d", this->tx_checksum_);
    ESP_LOGCONFIG(TAG, "  Device count: %d", this->devices_.size());
}

void UARTExComponent::setup()
{
    if (this->tx_ctrl_pin_)
    {
        this->tx_ctrl_pin_->setup();
        this->tx_ctrl_pin_->digital_write(false);
    }
    if (this->rx_checksum_ != CHECKSUM_NONE) this->rx_parser_.set_checksum_len(1);
    if (this->rx_checksum_2_ != CHECKSUM_NONE) this->rx_parser_.set_checksum_len(2);
    this->rx_time_ = get_time();
    this->tx_time_ = get_time();
    if (this->rx_header_.has_value()) this->rx_parser_.add_headers(this->rx_header_.value());
    if (this->rx_footer_.has_value()) this->rx_parser_.add_footers(this->rx_footer_.value());
    if (this->error_) this->error_->publish_state("None");
    if (this->version_) this->version_->publish_state(UARTEX_VERSION);
    ESP_LOGI(TAG, "Initaialize.");
    publish_log("Boot");
}

void UARTExComponent::loop()
{
    read_from_uart();
    publish_to_devices();
    write_to_uart();
}

void UARTExComponent::read_from_uart()
{
    this->rx_parser_.clear();
    unsigned long timer = get_time();
    if (!this->available()) return;
    while (elapsed_time(timer) < this->conf_rx_timeout_)
    {
        while (this->available())
        {
            uint8_t byte;
            if (this->read_byte(&byte))
            {
                if (this->rx_parser_.parse_byte(byte)) return;
                if (validate_data() == ERROR_NONE) return;
                timer = get_time();
            }
        }
        delay(1);
    }
}

void UARTExComponent::publish_to_devices()
{
    if (!this->rx_parser_.available()) return;
    if (!verify_data()) return;
    verify_ack();
    publish_data();
    this->rx_time_ = get_time();
}

bool UARTExComponent::verify_ack()
{
    if (!is_tx_cmd_pending()) return false;
    if (!equal(this->rx_parser_.data(), current_tx_cmd()->ack)) return false;
    tx_cmd_result(true);
    ESP_LOGD(TAG, "Ack: %s, Gap Time: %lums", to_hex_string(this->rx_parser_.buffer()).c_str(), elapsed_time(this->tx_time_));
    return true;
}

void UARTExComponent::publish_data()
{
    bool found = false;
    if (this->on_read_f_.has_value()) (*this->on_read_f_)(&this->rx_parser_.buffer()[0], this->rx_parser_.buffer().size());
    publish_log("[R]" + to_hex_string(this->rx_parser_.buffer()));
    for (UARTExDevice* device : this->devices_)
    {
        if (device->parse_data(this->rx_parser_.data()))
        {
            found = true;
        }
    }
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "Receive data-> %s, Gap Time: %lums", to_hex_string(this->rx_parser_.buffer()).c_str(), elapsed_time(this->rx_time_));
    if (!found) ESP_LOGVV(TAG, "Notfound data-> %s", to_hex_string(this->rx_parser_.buffer()).c_str());
#endif
#ifdef ESPHOME_LOG_HAS_VERBOSE
    ESP_LOGV(TAG, "Receive data-> %s, Gap Time: %lums", to_hex_string(this->rx_parser_.buffer()).c_str(), elapsed_time(this->rx_time_));
    if (!found) ESP_LOGV(TAG, "Notfound data-> %s", to_hex_string(this->rx_parser_.buffer()).c_str());
#endif
}

void UARTExComponent::dequeue_tx_data_from_devices()
{
    for (UARTExDevice* device : this->devices_)
    {
        const cmd_t *cmd = device->dequeue_tx_cmd();
        if (cmd != nullptr) enqueue_tx_data({device, cmd}, false);
        
        const cmd_t *cmd_low_priority = device->dequeue_tx_cmd_low_priority();
        if (cmd_low_priority != nullptr) enqueue_tx_data({device, cmd_low_priority}, true);
    }
}

void UARTExComponent::write_to_uart()
{
    if (elapsed_time(this->rx_time_) < this->conf_tx_delay_) return;
    if (elapsed_time(this->tx_time_) < this->conf_tx_delay_) return;
    if (is_tx_cmd_pending() && elapsed_time(this->tx_time_) < this->conf_tx_timeout_) return;
    if (retry_tx_data()) return;
    write_tx_data();
}

bool UARTExComponent::retry_tx_data()
{
    if (!is_tx_cmd_pending()) return false;
    if (this->conf_tx_retry_cnt_ <= this->tx_retry_cnt_)
    {
        tx_cmd_result(false);
        ESP_LOGD(TAG, "Retry fail.");
        publish_error(ERROR_ACK);
        return false;
    }
    ESP_LOGD(TAG, "Retry count: %d", this->tx_retry_cnt_);
    write_tx_cmd();
    return true;
}

void UARTExComponent::write_tx_data()
{
    dequeue_tx_data_from_devices();
    if (!this->tx_queue_.empty())
    {
        this->current_tx_data_ = this->tx_queue_.front();
        this->tx_queue_.pop();
        write_tx_cmd();
    }
    else if (!this->tx_queue_low_priority_.empty())
    {
        this->current_tx_data_ = this->tx_queue_low_priority_.front();
        this->tx_queue_low_priority_.pop();
        write_tx_cmd();
    }
}

void UARTExComponent::write_tx_cmd()
{
    unsigned long timer = get_time();
    std::vector<uint8_t> command;
    if (this->tx_ctrl_pin_) this->tx_ctrl_pin_->digital_write(true);
    if (this->tx_header_.has_value())
    {
        command.insert(command.end(), this->tx_header_.value().begin(), this->tx_header_.value().end());
        //write_data(this->tx_header_.value());
    }
    command.insert(command.end(), current_tx_cmd()->data.begin(), current_tx_cmd()->data.end());
    //write_data(current_tx_cmd()->data);
    if (this->tx_checksum_ != CHECKSUM_NONE || this->tx_checksum_2_ != CHECKSUM_NONE)
    {
        std::vector<uint8_t> checksum = get_tx_checksum(current_tx_cmd()->data);
        command.insert(command.end(), checksum.begin(), checksum.end());
        //write_data(get_tx_checksum(current_tx_cmd()->data));
    }
    if (this->tx_footer_.has_value())
    {
        command.insert(command.end(), this->tx_footer_.value().begin(), this->tx_footer_.value().end());
        //write_data(this->tx_footer_.value());
    }
    write_data(command);
    write_flush();
    if (this->tx_ctrl_pin_) this->tx_ctrl_pin_->digital_write(false);
    this->tx_retry_cnt_++;
    this->tx_time_ = get_time();
    if (current_tx_cmd()->ack.size() == 0) tx_cmd_result(true);
    if (this->on_write_f_.has_value()) (*this->on_write_f_)(&command[0], command.size());
    publish_log("[W]" + to_hex_string(command));
}

void UARTExComponent::write_data(const uint8_t data)
{
    this->write_byte(data);
    ESP_LOGD(TAG, "Write byte-> 0x%02X", data);
}

void UARTExComponent::write_data(const std::vector<uint8_t> &data)
{
    this->write_array(data);
    ESP_LOGD(TAG, "Write array-> %s", to_hex_string(data).c_str());
}

void UARTExComponent::enqueue_tx_data(const tx_data_t data, bool low_priority)
{
    if (low_priority) this->tx_queue_low_priority_.push(data);
    else this->tx_queue_.push(data);
}

void UARTExComponent::write_command(cmd_t cmd)
{
    command_ = cmd;
    const cmd_t* ptr = &command_.value();
    enqueue_tx_data({nullptr, ptr}, false);
}

void UARTExComponent::write_flush()
{
    this->flush();
    ESP_LOGD(TAG, "Flush.");
}

void UARTExComponent::register_device(UARTExDevice *device)
{
    this->devices_.push_back(device);
}

void UARTExComponent::set_tx_delay(uint16_t tx_delay)
{
    this->conf_tx_delay_ = tx_delay;
}

void UARTExComponent::set_tx_timeout(uint16_t timeout)
{
    this->conf_tx_timeout_ = timeout;
}

void UARTExComponent::set_tx_retry_cnt(uint16_t tx_retry_cnt)
{
    this->conf_tx_retry_cnt_ = tx_retry_cnt;
}

void UARTExComponent::set_rx_timeout(uint16_t timeout)
{
    this->conf_rx_timeout_ = timeout;
}

void UARTExComponent::set_tx_ctrl_pin(InternalGPIOPin *pin)
{
    this->tx_ctrl_pin_ = pin;
}

bool UARTExComponent::is_tx_cmd_pending()
{
    if (current_tx_cmd()) return true;
    return false;
}

void UARTExComponent::tx_cmd_result(bool result)
{
    clear_tx_data();
}

void UARTExComponent::clear_tx_data()
{
    this->current_tx_data_.device = nullptr;
    this->current_tx_data_.cmd = nullptr;
    this->tx_retry_cnt_ = 0;
}

const cmd_t* UARTExComponent::current_tx_cmd()
{
    return this->current_tx_data_.cmd;
}

ERROR UARTExComponent::validate_data()
{
    if (this->rx_parser_.data().size() == 0)
    {
        return ERROR_SIZE;
    }
    if (this->rx_header_.has_value() && this->rx_parser_.parse_header() == false)
    {
        return ERROR_HEADER;
    }
    if (this->rx_footer_.has_value() && this->rx_parser_.parse_footer() == false)
    {
        return ERROR_FOOTER;
    }
    if ((this->rx_checksum_ != CHECKSUM_NONE || this->rx_checksum_2_ != CHECKSUM_NONE) && !this->rx_parser_.verify_checksum(get_rx_checksum(this->rx_parser_.data())))
    {
        return ERROR_CHECKSUM;
    }
    if (!this->rx_footer_.has_value() && this->rx_checksum_ == CHECKSUM_NONE && this->rx_checksum_2_ == CHECKSUM_NONE)
    {
        return ERROR_TIMEOUT;
    }
    return ERROR_NONE;
}

bool UARTExComponent::verify_data()
{
    ERROR error = validate_data();
    publish_error(error);
    return (error == ERROR_NONE || error == ERROR_TIMEOUT);
}

bool UARTExComponent::publish_error(ERROR error_code)
{
    bool error = true;
    switch(error_code)
    {
    case ERROR_SIZE:
        ESP_LOGW(TAG, "[Read] Size error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_SIZE) this->error_->publish_state("Size Error");
        break;
    case ERROR_HEADER:
        ESP_LOGW(TAG, "[Read] Header error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_HEADER) this->error_->publish_state("Header Error");
        break;
    case ERROR_FOOTER:
        ESP_LOGW(TAG, "[Read] Footer error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_FOOTER) this->error_->publish_state("Footer Error");
        break;
    case ERROR_CHECKSUM:
        ESP_LOGW(TAG, "[Read] Checksum error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_CHECKSUM) this->error_->publish_state("Checksum Error");
        break;
    case ERROR_TIMEOUT:
        ESP_LOGW(TAG, "[Read] Timeout error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_TIMEOUT) this->error_->publish_state("Timeout Error");
        break;
    case ERROR_ACK:
        ESP_LOGW(TAG, "[Read] Ack error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_ACK) this->error_->publish_state("Ack Error");
        break;
    case ERROR_NONE:
        if (this->error_ && this->error_code_ != ERROR_NONE) this->error_->publish_state("None");
        error = false;
        break;
    }
    this->error_code_ = error_code;
    return error;
}

void UARTExComponent::publish_log(std::string msg)
{
    if (this->log_ == nullptr) return;
    if (this->last_log_ == msg)
    {
        this->log_->publish_state(msg + " (" + std::to_string(++this->log_count_) + ")");
    }
    else
    {
        this->log_count_ = 0;
        this->last_log_ = msg;
        this->log_->publish_state(msg);
    }
}

void UARTExComponent::set_rx_header(std::vector<uint8_t> header)
{
    this->rx_header_ = header;
}

void UARTExComponent::set_rx_footer(std::vector<uint8_t> footer)
{
    this->rx_footer_ = footer;
}

void UARTExComponent::set_tx_header(std::vector<uint8_t> header)
{
    this->tx_header_ = header;
}

void UARTExComponent::set_tx_footer(std::vector<uint8_t> footer)
{
    this->tx_footer_ = footer;
}

void UARTExComponent::set_rx_checksum(CHECKSUM checksum)
{
    this->rx_checksum_ = checksum;
}

void UARTExComponent::set_rx_checksum(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f)
{
    this->rx_checksum_f_ = f;
    this->rx_checksum_ = CHECKSUM_CUSTOM;
}

void UARTExComponent::set_tx_checksum(CHECKSUM checksum)
{
    this->tx_checksum_ = checksum;
}

void UARTExComponent::set_tx_checksum(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f)
{
    this->tx_checksum_f_ = f;
    this->tx_checksum_ = CHECKSUM_CUSTOM;
}

void UARTExComponent::set_rx_checksum_2(CHECKSUM checksum)
{
    this->rx_checksum_2_ = checksum;
}

void UARTExComponent::set_rx_checksum_2(std::function<std::vector<uint8_t>(const uint8_t *data, const uint16_t len)> &&f)
{
    this->rx_checksum_f_2_ = f;
    this->rx_checksum_2_ = CHECKSUM_CUSTOM;
}

void UARTExComponent::set_tx_checksum_2(CHECKSUM checksum)
{
    this->tx_checksum_2_ = checksum;
}

void UARTExComponent::set_tx_checksum_2(std::function<std::vector<uint8_t>(const uint8_t *data, const uint16_t len)> &&f)
{
    this->tx_checksum_f_2_ = f;
    this->tx_checksum_2_ = CHECKSUM_CUSTOM;
}

std::vector<uint8_t> UARTExComponent::get_rx_checksum(const std::vector<uint8_t> &data)
{
    if (this->rx_checksum_f_.has_value())
    {
        uint8_t crc = (*this->rx_checksum_f_)(&data[0], data.size());
        return { crc };
    }
    else if (this->rx_checksum_f_2_.has_value())
    {
        return (*this->rx_checksum_f_2_)(&data[0], data.size());
    }
    else
    {
        std::vector<uint8_t> header = this->rx_header_.value_or(std::vector<uint8_t>{});
        if (this->rx_checksum_ != CHECKSUM_NONE)
        {
            uint8_t crc = get_checksum(this->rx_checksum_, header, data) & 0xFF;
            return { crc };
        }
        else if (this->rx_checksum_2_ != CHECKSUM_NONE)
        {
            uint16_t crc = get_checksum(this->rx_checksum_2_, header, data);
            return { (uint8_t)(crc >> 8), (uint8_t)(crc & 0xFF) };
        }
    }
    return {};
}

std::vector<uint8_t> UARTExComponent::get_tx_checksum(const std::vector<uint8_t> &data)
{
    if (this->tx_checksum_f_.has_value())
    {
        uint8_t crc = (*this->tx_checksum_f_)(&data[0], data.size());
        return { crc };
    }
    else if (this->tx_checksum_f_2_.has_value())
    {
        return (*this->tx_checksum_f_2_)(&data[0], data.size());
    }
    else
    {
        std::vector<uint8_t> header = this->tx_header_.value_or(std::vector<uint8_t>{});
        if (this->tx_checksum_ != CHECKSUM_NONE)
        {
            uint8_t crc = get_checksum(this->tx_checksum_, header, data) & 0xFF;
            return { crc };
        }
        else if (this->tx_checksum_2_ != CHECKSUM_NONE)
        {
            uint16_t crc = get_checksum(this->tx_checksum_2_, header, data);
            return { (uint8_t)(crc >> 8), (uint8_t)(crc & 0xFF) };
        }
    }
    return {};
}

uint16_t UARTExComponent::get_checksum(CHECKSUM checksum, const std::vector<uint8_t> &header, const std::vector<uint8_t> &data)
{
    uint16_t crc = 0;
    switch(checksum)
    {
    case CHECKSUM_ADD:
        for (uint8_t byte : header) { crc += byte; }
        for (uint8_t byte : data) { crc += byte; }
        break;
    case CHECKSUM_XOR:
        for (uint8_t byte : header) { crc ^= byte; }
        for (uint8_t byte : data) { crc ^= byte; }
        break;
    case CHECKSUM_NONE:
        break;
    }
    return crc;
}

}  // namespace uartex
}  // namespace esphome