#include "uartex.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace uartex {
static const char *TAG = "uartex";
void UARTExComponent::dump_config()
{
    ESP_LOGCONFIG(TAG, "  RX Receive Timeout: %d", conf_rx_timeout_);
    ESP_LOGCONFIG(TAG, "  TX Transmission Timeout: %d", conf_tx_timeout_);
    ESP_LOGCONFIG(TAG, "  TX Retry Count: %d", conf_tx_retry_cnt_);
    if (tx_ctrl_pin_)   LOG_PIN("  TX Ctrl Pin: ", tx_ctrl_pin_);
    if (rx_header_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_header: %s", to_hex_string(rx_header_.value()).c_str());
    if (rx_footer_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_footer: %s", to_hex_string(rx_footer_.value()).c_str());
    if (tx_header_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_header: %s", to_hex_string(tx_header_.value()).c_str());
    if (tx_footer_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_footer: %s", to_hex_string(tx_footer_.value()).c_str());
    ESP_LOGCONFIG(TAG, "  Data rx_checksum: %d", rx_checksum_);
    ESP_LOGCONFIG(TAG, "  Data tx_checksum: %d", tx_checksum_);
    ESP_LOGCONFIG(TAG, "  Device count: %d", devices_.size());
}

void UARTExComponent::setup()
{
    if (this->tx_ctrl_pin_)
    {
        this->tx_ctrl_pin_->setup();
        this->tx_ctrl_pin_->digital_write(false);
    }
    if (rx_checksum_) rx_parser_.set_checksum_len(1);
    if (rx_checksum_2_) rx_parser_.set_checksum_len(2);
    rx_time_ = get_time();
    tx_time_ = get_time();
    if (rx_header_.has_value()) rx_parser_.add_headers(rx_header_.value());
    if (rx_footer_.has_value()) rx_parser_.add_footers(rx_footer_.value());

    if (this->version_) this->version_->publish_state(UARTEX_VERSION);
    ESP_LOGI(TAG, "Initaialize.");
}

void UARTExComponent::loop()
{
    read_from_uart();
    publish_to_devices();
    write_to_uart();
}

void UARTExComponent::read_from_uart()
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
            if (validate_data() == ERR_NONE) valid_data = true;
            timer = get_time();
        }
        if (valid_data) break;
        delay(1);
    }
}

void UARTExComponent::publish_to_devices()
{
    if (rx_parser_.buffer().size() == 0) return;
    if (validate_data(true) != ERR_NONE) return;
    if (validate_ack()) return;
    publish_data();
    rx_time_ = get_time();
}

bool UARTExComponent::validate_ack()
{
    if (!is_have_tx_cmd()) return false;
    if (tx_device() == nullptr) return false;
    if (!tx_device()->equal(rx_parser_.data(), tx_cmd()->ack)) return false;
    ack_tx_data(true);
    ESP_LOGD(TAG, "Ack: %s, Gap Time: %lums", to_hex_string(rx_parser_.buffer()).c_str(), elapsed_time(tx_time_));
    //return true;
    return false;
}

void UARTExComponent::publish_data()
{
    bool found = false;
    for (UARTExDevice* device : this->devices_)
    {
        if (device->parse_data(rx_parser_.data()))
        {
            found = true;
        }
    }
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "Receive data-> %s, Gap Time: %lums", to_hex_string(rx_parser_.buffer()).c_str(), elapsed_time(rx_time_));
#endif
#ifdef ESPHOME_LOG_HAS_VERBOSE
    if (!found) ESP_LOGV(TAG, "Notfound data-> %s", to_hex_string(rx_parser_.buffer()).c_str());
#endif
}

void UARTExComponent::pop_tx_data()
{
    for (UARTExDevice* device : this->devices_)
    {
        const cmd_t *cmd = device->pop_tx_cmd();
        if (cmd == nullptr) continue;
        if (cmd->ack.size() == 0)   push_tx_data_late({device, cmd});
        else                        push_tx_data({device, cmd});
    }
}

void UARTExComponent::write_to_uart()
{
    if (elapsed_time(rx_time_) < conf_tx_delay_) return;
    if (elapsed_time(tx_time_) < conf_tx_delay_) return;
    if (elapsed_time(tx_time_) < conf_tx_timeout_) return;
    if (retry_tx_cmd()) return;
    write_tx_data();
}

bool UARTExComponent::retry_tx_cmd()
{
    if (!is_have_tx_cmd()) return false;
    if (conf_tx_retry_cnt_ <= tx_retry_cnt_)
    {
        ack_tx_data(false);
        ESP_LOGD(TAG, "Retry fail.");
        return false;
    }
    ESP_LOGD(TAG, "Retry count: %d", tx_retry_cnt_);
    write_tx_cmd();
    return true;
}

void UARTExComponent::write_tx_data()
{
    pop_tx_data();
    if (!tx_queue_.empty())
    {
        tx_data_ = tx_queue_.front();
        tx_queue_.pop();
        write_tx_cmd();
    }
    else if (!tx_queue_late_.empty())
    {
        tx_data_ = tx_queue_late_.front();
        tx_queue_late_.pop();
        write_tx_cmd();
    }
}

void UARTExComponent::write_tx_cmd()
{
    unsigned long timer = get_time();
    if (tx_ctrl_pin_) tx_ctrl_pin_->digital_write(true);
    if (tx_header_.has_value()) write_data(tx_header_.value());
    write_data(tx_cmd()->data);
    if (tx_checksum_) write_data(get_tx_checksum(tx_cmd()->data));
    if (tx_checksum_2_) write_data(get_tx_checksum_2(tx_cmd()->data));
    if (tx_footer_.has_value()) write_data(tx_footer_.value());
    write_flush(timer);
    if (tx_ctrl_pin_) tx_ctrl_pin_->digital_write(false);
    tx_retry_cnt_++;
    tx_time_ = get_time();
    if (tx_cmd()->ack.size() == 0) ack_tx_data(true);
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

void UARTExComponent::push_tx_data(const tx_data data)
{
    tx_queue_.push(data);
}

void UARTExComponent::push_tx_data_late(const tx_data data)
{
    tx_queue_late_.push(data);
}

void UARTExComponent::write_flush(const unsigned long timer)
{
    this->flush();
    ESP_LOGD(TAG, "Flushing... (%lums)", elapsed_time(timer));
}

void UARTExComponent::register_device(UARTExDevice *device)
{
    devices_.push_back(device);
}

void UARTExComponent::set_tx_delay(uint16_t tx_delay)
{
    conf_tx_delay_ = tx_delay;
}

void UARTExComponent::set_tx_timeout(uint16_t timeout)
{
    conf_tx_timeout_ = timeout;
}

void UARTExComponent::set_tx_retry_cnt(uint16_t tx_retry_cnt)
{
    conf_tx_retry_cnt_ = tx_retry_cnt;
}

void UARTExComponent::set_rx_timeout(uint16_t timeout)
{
    conf_rx_timeout_ = timeout;
}

void UARTExComponent::set_tx_ctrl_pin(InternalGPIOPin *pin)
{
    tx_ctrl_pin_ = pin;
}

bool UARTExComponent::is_have_tx_cmd()
{
    if (tx_data_.cmd) return true;
    return false;
}

void UARTExComponent::ack_tx_data(bool ok)
{
    if (tx_data_.device)
    {
        if (ok) tx_data_.device->ack_ok();
        else    tx_data_.device->ack_ng();
    }
    clear_tx_data();
}

void UARTExComponent::clear_tx_data()
{
    tx_data_.device = nullptr;
    tx_data_.cmd = nullptr;
    tx_retry_cnt_ = 0;
}

const cmd_t* UARTExComponent::tx_cmd()
{
    return tx_data_.cmd;
}

UARTExDevice* UARTExComponent::tx_device()
{
    return tx_data_.device;
}

unsigned long UARTExComponent::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long UARTExComponent::get_time()
{
    return millis();
}

ValidateCode UARTExComponent::validate_data(bool log)
{
    if (rx_parser_.data().size() == 0)
    {
        if (log) ESP_LOGW(TAG, "[Read] Size error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        return ERR_SIZE;
    }
    if (rx_header_.has_value() && rx_parser_.parse_header() == false)
    {
        if (log) ESP_LOGW(TAG, "[Read] Header error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        return ERR_HEADER;
    }
    if (rx_footer_.has_value() && rx_parser_.parse_footer() == false)
    {
        if (log) ESP_LOGW(TAG, "[Read] Footer error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        return ERR_FOOTER;
    }
    uint8_t crc = get_rx_checksum(rx_parser_.data());
    if (rx_checksum_ && crc != rx_parser_.get_checksum())
    {
        if (log) ESP_LOGW(TAG, "[Read] Checksum error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        return ERR_CHECKSUM;
    }
    crc = get_rx_checksum_2(rx_parser_.data());
    if (rx_checksum_2_ && crc != rx_parser_.get_checksum_2())
    {
        if (log) ESP_LOGW(TAG, "[Read] Checksum error: %s", to_hex_string(rx_parser_.buffer()).c_str());
        return ERR_CHECKSUM;
    }
    return ERR_NONE;
}

void UARTExComponent::set_rx_header(std::vector<uint8_t> header)
{
    rx_header_ = header;
}

void UARTExComponent::set_rx_footer(std::vector<uint8_t> footer)
{
    rx_footer_ = footer;
}

void UARTExComponent::set_tx_header(std::vector<uint8_t> header)
{
    tx_header_ = header;
}

void UARTExComponent::set_tx_footer(std::vector<uint8_t> footer)
{
    tx_footer_ = footer;
}

void UARTExComponent::set_rx_checksum(Checksum checksum)
{
    rx_checksum_ = checksum;
}

void UARTExComponent::set_rx_checksum_2(Checksum checksum)
{
    rx_checksum_2_ = checksum;
}

void UARTExComponent::set_rx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f)
{
    rx_checksum_f_ = f;
    rx_checksum_ = CHECKSUM_CUSTOM;
}

void UARTExComponent::set_rx_checksum_2_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)> &&f)
{
    rx_checksum_f_2_ = f;
    rx_checksum_2_ = CHECKSUM_CUSTOM;
}

void UARTExComponent::set_tx_checksum(Checksum checksum)
{
    tx_checksum_ = checksum;
}

void UARTExComponent::set_tx_checksum_2(Checksum checksum)
{
    tx_checksum_2_ = checksum;
}

void UARTExComponent::set_tx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f)
{
    tx_checksum_f_ = f;
    tx_checksum_ = CHECKSUM_CUSTOM;
}

void UARTExComponent::set_tx_checksum_2_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)> &&f)
{
    tx_checksum_f_2_ = f;
    tx_checksum_2_ = CHECKSUM_CUSTOM;
}

uint8_t UARTExComponent::get_rx_checksum(const std::vector<uint8_t> &data) const
{
    if (this->rx_checksum_f_.has_value())
    {
        return (*rx_checksum_f_)(&data[0], data.size());
    }
    else
    {
        uint8_t crc = 0;
        switch(rx_checksum_)
        {
        case CHECKSUM_ADD:
            if (this->rx_header_.has_value())
            {
                for (uint8_t byte : this->rx_header_.value()) { crc += byte; }
            }
            for (uint8_t byte : data) { crc += byte; }
            break;
        case CHECKSUM_XOR:
            if (this->rx_header_.has_value())
            {
                for (uint8_t byte : this->rx_header_.value()) { crc ^= byte; }
            }
            for (uint8_t byte : data) { crc ^= byte; }
            break;
        case CHECKSUM_NONE:
        case CHECKSUM_CUSTOM:
            break;
        }
        return crc;
    }
}


uint8_t UARTExComponent::get_rx_checksum_2(const std::vector<uint8_t> &data) const
{
    uint8_t checksum = get_rx_checksum(data);
    if (this->rx_checksum_f_2_.has_value())
    {
        return (*rx_checksum_f_2_)(&data[0], data.size(), checksum);
    }
    else
    {
        uint8_t crc = 0;
        switch(rx_checksum_2_)
        {
        case CHECKSUM_ADD:
            if (this->rx_header_.has_value())
            {
                for (uint8_t byte : this->rx_header_.value()) { crc += byte; }
            }
            for (uint8_t byte : data) { crc += byte; }
            crc += checksum;
            break;
        case CHECKSUM_XOR:
            if (this->rx_header_.has_value())
            {
                for (uint8_t byte : this->rx_header_.value()) { crc ^= byte; }
            }
            for (uint8_t byte : data) { crc ^= byte; }
            crc ^= checksum;
            break;
        case CHECKSUM_NONE:
        case CHECKSUM_CUSTOM:
            break;
        }
        return crc;
    }
}

uint8_t UARTExComponent::get_tx_checksum(const std::vector<uint8_t> &data) const
{
    if (this->tx_checksum_f_.has_value())
    {
        return (*tx_checksum_f_)(&data[0], data.size());
    }
    else
    {
        uint8_t crc = 0;
        switch(tx_checksum_)
        {
        case CHECKSUM_ADD:
            if (this->tx_header_.has_value())
            {
                for (uint8_t byte : this->tx_header_.value()) { crc += byte; }
            }
            for (uint8_t byte : data) { crc += byte; }
            break;
        case CHECKSUM_XOR:
            if (this->tx_header_.has_value())
            {
                for (uint8_t byte : this->tx_header_.value()) { crc ^= byte; }
            }
            for (uint8_t byte : data) { crc ^= byte; }
            break;
        case CHECKSUM_NONE:
        case CHECKSUM_CUSTOM:
            break;
        }
        return crc;
    }
}


uint8_t UARTExComponent::get_tx_checksum_2(const std::vector<uint8_t> &data) const
{
    uint8_t checksum = get_tx_checksum(data);
    if (this->tx_checksum_f_2_.has_value())
    {
        return (*tx_checksum_f_2_)(&data[0], data.size(), checksum);
    }
    else
    {
        uint8_t crc = 0;
        switch(tx_checksum_2_)
        {
        case CHECKSUM_ADD:
            if (this->tx_header_.has_value())
            {
                for (uint8_t byte : this->tx_header_.value()) { crc += byte; }
            }
            for (uint8_t byte : data) { crc += byte; }
            crc += checksum;
            break;
        case CHECKSUM_XOR:
            if (this->tx_header_.has_value())
            {
                for (uint8_t byte : this->tx_header_.value()) { crc ^= byte; }
            }
            for (uint8_t byte : data) { crc ^= byte; }
            crc ^= checksum;
            break;
        case CHECKSUM_NONE:
        case CHECKSUM_CUSTOM:
            break;
        }
        return crc;
    }
}
}  // namespace uartex
}  // namespace esphome