#include "uartex.h"

namespace esphome {
namespace uartex {
static const char *TAG = "uartex";
void UARTExComponent::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "rx_timeout", this->conf_rx_timeout_);
    log_config(TAG, "rx_length", this->conf_rx_length_);
    log_config(TAG, "tx_timeout", this->conf_tx_timeout_);
    log_config(TAG, "tx_delay", this->conf_tx_delay_);
    log_config(TAG, "tx_retry_cnt", this->conf_tx_retry_cnt_);
    if (this->rx_header_.has_value()) log_config(TAG, "rx_header", this->rx_header_.value().data);
    if (this->rx_header_.has_value()) log_config(TAG, "rx_header mask", this->rx_header_.value().mask);
    if (this->rx_footer_.has_value()) log_config(TAG, "rx_footer", this->rx_footer_.value());
    if (this->tx_header_.has_value()) log_config(TAG, "tx_header", this->tx_header_.value());
    if (this->tx_footer_.has_value()) log_config(TAG, "tx_footer", this->tx_footer_.value());
    log_config(TAG, "rx_checksum", (uint16_t)this->rx_checksum_);
    log_config(TAG, "rx_checksum2", (uint16_t)this->rx_checksum_2_);
    log_config(TAG, "tx_checksum", (uint16_t)this->tx_checksum_);
    log_config(TAG, "tx_checksum2", (uint16_t)this->tx_checksum_2_);
    log_config(TAG, "uartex count", (uint16_t)this->devices_.size());
    if (this->tx_ctrl_pin_) LOG_PIN("tx_ctrl_pin: ", this->tx_ctrl_pin_);
#endif
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
    this->rx_timer_ = get_time();
    if (this->rx_header_.has_value())
    {
        this->rx_parser_.add_headers(this->rx_header_.value().data);
        this->rx_parser_.add_header_masks(this->rx_header_.value().mask);
    }
    if (this->rx_footer_.has_value()) this->rx_parser_.add_footers(this->rx_footer_.value());
    this->rx_parser_.set_total_len(this->conf_rx_length_);
    this->rx_parser_.set_buffer_len(this->parent_->get_rx_buffer_size());
    if (this->error_) this->error_->publish_state("None");
    if (this->version_) this->version_->publish_state(UARTEX_VERSION);
    ESP_LOGI(TAG, "Initaialize");
    publish_log(std::string("Boot ") + UARTEX_VERSION);

    if (this->tcp_port_ > 0) {
        this->server_ = new network::AsyncServer(this->tcp_port_);
        this->server_->onClient([this](void *arg, network::AsyncClient *client) {
            if (this->client_ != nullptr) {
                ESP_LOGW(TAG, "Already connected, disconnecting new client");
                client->close();
                return;
            }
            this->client_ = client;
            ESP_LOGI(TAG, "New client connected");

            this->client_->onDisconnect([this](void *arg, network::AsyncClient *client) {
                ESP_LOGI(TAG, "Client disconnected");
                this->client_ = nullptr;
            });

            this->client_->onData([this](void *arg, network::AsyncClient *client, void *data, size_t len) {
                if (this->tcp_mode_ == TCP_MODE_READ_WRITE) {
                    this->write_array((uint8_t*)data, len);
                }
            });
        });
        this->server_->begin();
    }
}

void UARTExComponent::loop()
{
    if (read_from_uart()) publish_to_devices();
    else if(!this->rx_processing_) write_to_uart();
}

bool UARTExComponent::read_from_uart()
{
    if (this->rx_priority_ == PRIORITY_DATA)
    {
        this->rx_processing_ = false;
        this->rx_parser_.clear();
        if (this->available())
        {
            this->rx_timer_ = get_time();
            while (elapsed_time(this->rx_timer_) < this->conf_rx_timeout_)
            {
                while (this->available())
                {
                    if (parse_bytes()) return true;
                }
                delay(1);
            }
        }
    }
    else if (this->rx_priority_ == PRIORITY_LOOP)
    {
        if (!this->rx_processing_ || (!this->available() && elapsed_time(this->rx_timer_) > this->conf_rx_timeout_))
        {
            this->rx_processing_ = false;
            this->rx_parser_.clear();
            this->rx_timer_ = get_time();
        }
        unsigned long timer = get_time();
        while (this->available() && elapsed_time(timer) < this->conf_rx_timeout_)
        {
            this->rx_processing_ = true;
            if (parse_bytes()) return true;
        }
    }
    return false;
}

bool UARTExComponent::parse_bytes()
{
    uint8_t byte = 0x00;
    if (this->read_byte(&byte))
    {
        if (this->rx_parser_.parse_byte(byte)) return true;
        if (!this->rx_parser_.has_footer() && validate_data() == ERROR_NONE) return true;
        this->rx_timer_ = get_time();
    }
    return false;
}

void UARTExComponent::publish_to_devices()
{
    this->rx_processing_ = false;
    if (!this->rx_parser_.available()) return;
    if (!verify_data()) return;
    verify_ack();
    publish_data();
    this->rx_time_ = get_time();
}

bool UARTExComponent::verify_ack()
{
    if (!is_tx_cmd_pending()) return false;
    if (!equal(this->rx_parser_.data(current_tx_cmd()->mask), current_tx_cmd()->ack)) return false;
    tx_cmd_result(true);
    ESP_LOGD(TAG, "Ack: %s, Gap Time: %lums", to_hex_string(this->rx_parser_.buffer()).c_str(), elapsed_time(this->tx_time_));
    return true;
}

void UARTExComponent::publish_data()
{
    auto& data = this->rx_parser_.data();
    this->read_callback_.call(&this->rx_parser_.buffer()[0], this->rx_parser_.buffer().size());
    publish_rx_log(this->rx_parser_.buffer());
    if (this->client_ && this->client_->can_send()) {
        this->client_->add((const char*)this->rx_parser_.buffer().data(), this->rx_parser_.buffer().size());
    }
    for (UARTExDevice* device : this->devices_)
    {
        device->parse_data(data);
    }
#ifdef ESPHOME_LOG_HAS_VERBOSE
    ESP_LOGV(TAG, "Receive data-> %s", to_hex_string(this->rx_parser_.buffer()).c_str());
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
        ESP_LOGD(TAG, "Retry failed");
        publish_error(ERROR_TX_TIMEOUT);
        this->error_callback_.call(ERROR_TX_TIMEOUT);
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
    std::vector<uint8_t> command;
    if (this->tx_ctrl_pin_) this->tx_ctrl_pin_->digital_write(true);
    if (this->tx_header_.has_value())
    {
        command.insert(command.end(), this->tx_header_.value().begin(), this->tx_header_.value().end());
    }
    command.insert(command.end(), current_tx_cmd()->data.begin(), current_tx_cmd()->data.end());
    if (this->tx_checksum_ != CHECKSUM_NONE || this->tx_checksum_2_ != CHECKSUM_NONE)
    {
        std::vector<uint8_t> checksum = get_tx_checksum(current_tx_cmd()->data);
        command.insert(command.end(), checksum.begin(), checksum.end());
    }
    if (this->tx_footer_.has_value())
    {
        command.insert(command.end(), this->tx_footer_.value().begin(), this->tx_footer_.value().end());
    }
    write_data(command);
    write_flush();
    if (this->tx_ctrl_pin_) this->tx_ctrl_pin_->digital_write(false);
    if (this->client_ && this->client_->can_send()) {
        this->client_->add((const char*)command.data(), command.size());
    }
    this->tx_retry_cnt_++;
    this->tx_time_ = get_time();
    if (current_tx_cmd()->ack.empty()) tx_cmd_result(true);
    this->write_callback_.call(&command[0], command.size());
    publish_tx_log(command);
}

void UARTExComponent::write_data(const uint8_t data)
{
    this->write_byte(data);
    ESP_LOGD(TAG, "Write byte-> 0x%02X", data);
}

void UARTExComponent::write_data(const std::vector<uint8_t> &data)
{
    this->write_array(data);
    ESP_LOGD(TAG, "Write data-> %s", to_hex_string(data).c_str());
}

void UARTExComponent::enqueue_tx_data(const tx_data_t data, bool low_priority)
{
    if (low_priority) this->tx_queue_low_priority_.push(data);
    else this->tx_queue_.push(data);
}

void UARTExComponent::write_command(cmd_t cmd)
{
    std::string name = "command_queue_" + std::to_string(tx_command_cnt_);
    write_command(name, cmd);
    if (++tx_command_cnt_ >= conf_tx_command_queue_size_) tx_command_cnt_ = 0;
}

void UARTExComponent::write_command(std::string name, cmd_t cmd)
{
    this->command_map_[name] = cmd;
    const cmd_t* ptr = &this->command_map_[name];
    enqueue_tx_data({nullptr, ptr}, false);
}

void UARTExComponent::write_flush()
{
    this->flush();
    ESP_LOGV(TAG, "Flush");
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

void UARTExComponent::set_tx_command_queue_size(uint16_t size)
{
    this->conf_tx_command_queue_size_ = size;
}

void UARTExComponent::set_rx_length(uint16_t rx_length)
{
    this->conf_rx_length_ = rx_length;
}

void UARTExComponent::set_rx_timeout(uint16_t timeout)
{
    this->conf_rx_timeout_ = timeout;
}

void UARTExComponent::set_tx_ctrl_pin(InternalGPIOPin *pin)
{
    this->tx_ctrl_pin_ = pin;
}

void UARTExComponent::set_tcp_port(uint16_t port)
{
    this->tcp_port_ = port;
}

void UARTExComponent::set_tcp_mode(TCP_MODE mode)
{
    this->tcp_mode_ = mode;
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
    auto& data = this->rx_parser_.data();
    if (data.empty())
    {
        return ERROR_SIZE;
    }
    if (this->conf_rx_length_ > 0 && this->conf_rx_length_ != this->rx_parser_.buffer().size())
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
    if ((this->rx_checksum_ != CHECKSUM_NONE || this->rx_checksum_2_ != CHECKSUM_NONE) && !this->rx_parser_.verify_checksum(get_rx_checksum(data, this->rx_parser_.header())))
    {
        return ERROR_CHECKSUM;
    }
    if (!this->rx_footer_.has_value() && this->conf_rx_length_ == 0 && this->rx_checksum_ == CHECKSUM_NONE && this->rx_checksum_2_ == CHECKSUM_NONE)
    {
        return ERROR_RX_TIMEOUT;
    }
    return ERROR_NONE;
}

bool UARTExComponent::verify_data()
{
    ERROR error = validate_data();
    publish_error(error);
    if (error != ERROR_NONE) this->error_callback_.call(error);
    return (error == ERROR_NONE || error == ERROR_RX_TIMEOUT);
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
    case ERROR_RX_TIMEOUT:
        ESP_LOGW(TAG, "[Read] Rx Timeout error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_RX_TIMEOUT) this->error_->publish_state("Rx Timeout Error");
        break;
    case ERROR_TX_TIMEOUT:
        ESP_LOGW(TAG, "[Read] Tx Timeout error: %s", to_hex_string(this->rx_parser_.buffer()).c_str());
        if (this->error_ && this->error_code_ != ERROR_TX_TIMEOUT) this->error_->publish_state("Tx Timeout Error");
        break;
    case ERROR_NONE:
        if (this->error_ && this->error_code_ != ERROR_NONE) this->error_->publish_state("None");
        error = false;
        break;
    }
    this->error_code_ = error_code;
    return error;
}

void UARTExComponent::publish_rx_log(const std::vector<unsigned char>& data)
{
    if (this->log_ == nullptr) return;
    if (this->log_ascii_)   publish_log("[R]" + to_ascii_string(data));
    else                    publish_log("[R]" + to_hex_string(data));
}

void UARTExComponent::publish_tx_log(const std::vector<unsigned char>& data)
{
    if (this->log_ == nullptr) return;
    if (this->log_ascii_)   publish_log("[W]" + to_ascii_string(data));
    else                    publish_log("[W]" + to_hex_string(data));
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

void UARTExComponent::set_rx_header(header_t header)
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

void UARTExComponent::set_rx_priority(PRIORITY priority)
{
    this->rx_priority_ = priority;
}

std::vector<uint8_t> UARTExComponent::get_rx_checksum(const std::vector<uint8_t> &data, const std::vector<uint8_t> &header)
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
    uint8_t temp = 0;
    switch(checksum)
    {
    case CHECKSUM_XOR:
        for (uint8_t byte : header) { crc ^= byte; }
        for (uint8_t byte : data) { crc ^= byte; }
        break;
    case CHECKSUM_ADD:
        for (uint8_t byte : header) { crc += byte; }
        for (uint8_t byte : data) { crc += byte; }
        break;
    case CHECKSUM_XOR_NO_HEADER:
        for (uint8_t byte : data) { crc ^= byte; }
        break;
    case CHECKSUM_ADD_NO_HEADER:
        for (uint8_t byte : data) { crc += byte; }
        break;
    case CHECKSUM_XOR_ADD:
        for (uint8_t byte : header)
        {
            crc += byte;
            temp ^= byte;
        }
        for (uint8_t byte : data)
        { 
            crc += byte;
            temp ^= byte;
        }
        crc += temp;
        crc = ((uint16_t)temp << 8) | (crc & 0xFF);
        break;
    case CHECKSUM_NONE:
    case CHECKSUM_CUSTOM:
        break;
    }
    return crc;
}

}  // namespace uartex
}  // namespace esphome