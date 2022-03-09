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
    if (ctrl_pin_)              LOG_PIN("  Ctrl Pin: ", ctrl_pin_);
    if (status_pin_)            LOG_PIN("  Status Pin: ", ctrl_pin_);
    if (rx_prefix_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_prefix: %s", to_hex_string(rx_prefix_.value()).c_str());
    if (rx_suffix_.has_value()) ESP_LOGCONFIG(TAG, "  Data rx_suffix: %s", to_hex_string(rx_suffix_.value()).c_str());
    if (tx_prefix_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_prefix: %s", to_hex_string(tx_prefix_.value()).c_str());
    if (tx_suffix_.has_value()) ESP_LOGCONFIG(TAG, "  Data tx_suffix: %s", to_hex_string(tx_suffix_.value()).c_str());
    ESP_LOGCONFIG(TAG, "  Data rx_checksum: %d", rx_checksum_);
    ESP_LOGCONFIG(TAG, "  Data tx_checksum: %d", tx_checksum_);
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
    if (this->status_pin_)
    {
        this->status_pin_->setup();
        this->status_pin_->digital_write(false);
    }

    if (rx_checksum_)   this->rx_checksum_len_++;

    rx_lastTime_ = get_time();
    tx_start_time_ = get_time();
    if (rx_prefix_.has_value()) parser_.add_headers(rx_prefix_.value());
    if (rx_suffix_.has_value()) parser_.add_footers(rx_suffix_.value());
    ESP_LOGI(TAG, "HW Serial Initaialize.");
}

void WallPadComponent::loop()
{
    read_from_serial();
    treat_recived_data();
    write_to_serial();
}

void WallPadComponent::read_from_serial()
{
    parser_.clear();
    unsigned long timer = get_time();
    while (elapsed_time(timer) < conf_rx_wait_)
    {
        while (this->hw_serial_->available())
        {
            if (parser_.parse_byte(this->hw_serial_->read())) return;
            if (validate_data() == ERR_NONE) return;
            timer = get_time();
        }
        delay(1);
    }
}

void WallPadComponent::treat_recived_data()
{
    if (parser_.buffer().size() == 0) return;
    if (validate_data(true) != ERR_NONE) return;
    if (validate_ack()) return;
    publish_data();
}

bool WallPadComponent::validate_ack()
{
    if (!tx_ack_wait_) return false;
    if (!is_have_writing_data()) return false;
    auto *device = get_writing_device();
    if (!device->equal(parser_.data(), get_writing_cmd()->ack, 0)) return false;
    device->ack_ok();
    clear_writing_data();
    ESP_LOGD(TAG, "Ack: %s, Gap Time: %lums", to_hex_string(parser_.buffer()).c_str(), elapsed_time(tx_start_time_));
    rx_lastTime_ = get_time();
    return true;
}

void WallPadComponent::publish_data()
{
#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    ESP_LOGVV(TAG, "Receive data-> %s, Gap Time: %lums", to_hex_string(parser_.buffer()).c_str(), elapsed_time(rx_lastTime_));
#endif

    bool found = false;
    for (auto *device : this->devices_)
    {
        if (device->parse_data(parser_.data()))
        {
            found = true;
        }
    }

#ifdef ESPHOME_LOG_HAS_VERBOSE
    if (!found) ESP_LOGV(TAG, "Notfound data-> %s", to_hex_string(parser_.buffer()).c_str());
#endif
    rx_lastTime_ = get_time();
}

void WallPadComponent::pop_command_to_write()
{
    for (auto *device : this->devices_)
    {
        if (device->is_have_command())
        {
            const cmd_hex_t *cmd = device->pop_command();
            if (cmd == nullptr) continue;
            if (cmd->ack.size() == 0)   write_next_late({device, cmd});
            else                        write_next({device, cmd});
        }
    }
}
void WallPadComponent::write_to_serial()
{
    if (tx_ack_wait_ && elapsed_time(tx_start_time_) > conf_tx_wait_) tx_ack_wait_ = false;
    if (elapsed_time(rx_lastTime_) < conf_tx_interval_) return;
    if (elapsed_time(tx_start_time_) < conf_tx_interval_) return;
    if (tx_ack_wait_) return;
    if (retry_write()) return;
    write_command();
}

bool WallPadComponent::retry_write()
{
    if (!is_have_writing_data()) return false;
    if (conf_tx_retry_cnt_ <= tx_retry_cnt_)
    {
        get_writing_device()->ack_ng();
        clear_writing_data();
        ESP_LOGD(TAG, "Retry fail.");
        return false;
    }
    ESP_LOGD(TAG, "Retry count: %d", tx_retry_cnt_);
    write_with_header(get_writing_cmd()->data);
    tx_ack_wait_ = true;
    tx_retry_cnt_++;
    return true;
}

void WallPadComponent::write_command()
{
    pop_command_to_write();
    if (!tx_queue_.empty() || !tx_queue_late_.empty())
    {
        if (!tx_queue_.empty())
        {
            writing_data_ = tx_queue_.front();
            tx_queue_.pop();
        }
        else
        {
            writing_data_ = tx_queue_late_.front();
            tx_queue_late_.pop();
        }
        write_with_header(writing_data_.cmd->data);
        if (writing_data_.cmd->ack.size() > 0)
        {
            tx_ack_wait_ = true;
            tx_retry_cnt_ = 1;
        }
        else
        {
            get_writing_device()->ack_ok();
            clear_writing_data();
        }
    }
}

void WallPadComponent::write_with_header(const std::vector<uint8_t> &data)
{
    tx_start_time_ = get_time();
    if (status_pin_) status_pin_->digital_write(true);
    if (ctrl_pin_) ctrl_pin_->digital_write(TX_ENABLE);
    if (tx_prefix_.has_value()) write_array(tx_prefix_.value());
    write_array(data);
    if (tx_checksum_) write_byte(make_tx_checksum(data));
    if (tx_suffix_.has_value()) write_array(tx_suffix_.value());
    if (ctrl_pin_)
    {
        flush();
        ctrl_pin_->digital_write(RX_ENABLE);
    }
    if (status_pin_) status_pin_->digital_write(false);
    tx_start_time_ = get_time();
}

void WallPadComponent::write_byte(uint8_t data)
{
    this->hw_serial_->write(data);
    ESP_LOGD(TAG, "Write byte-> 0x%02X", data);
}

void WallPadComponent::write_array(const std::vector<uint8_t> &data)
{
    this->hw_serial_->write(&data[0], data.size());
    ESP_LOGD(TAG, "Write array-> %s", to_hex_string(data).c_str());
}

void WallPadComponent::write_next(const write_data data)
{
    tx_queue_.push(data);
}

void WallPadComponent::write_next_late(const write_data data)
{
    tx_queue_late_.push(data);
}

void WallPadComponent::flush()
{
    this->hw_serial_->flush(true);
    ESP_LOGD(TAG, "Flushing... (%lums)", elapsed_time(tx_start_time_));
}
void WallPadComponent::register_device(WallPadDevice *device)
{
    devices_.push_back(device);
}
void WallPadComponent::set_tx_interval(num_t tx_interval)
{
    conf_tx_interval_ = tx_interval;
}
void WallPadComponent::set_tx_wait(num_t tx_wait)
{
    conf_tx_wait_ = tx_wait;
}
void WallPadComponent::set_tx_retry_cnt(num_t tx_retry_cnt)
{
    conf_tx_retry_cnt_ = tx_retry_cnt;
}
void WallPadComponent::set_ctrl_pin(InternalGPIOPin *pin)
{
    ctrl_pin_ = pin;
}
void WallPadComponent::set_status_pin(InternalGPIOPin *pin)
{
    status_pin_ = pin;
}
void WallPadComponent::set_tx_pin(InternalGPIOPin *tx_pin)
{
    tx_pin_ = tx_pin;
}
void WallPadComponent::set_rx_pin(InternalGPIOPin *rx_pin)
{
    rx_pin_ = rx_pin;
}
void WallPadComponent::set_model(Model model)
{
    conf_model_ = model;
}
Model WallPadComponent::get_model()
{
    return conf_model_;
}
bool WallPadComponent::is_have_writing_data()
{
    if (writing_data_.device) return true;
    return false;
}
void WallPadComponent::clear_writing_data()
{
    writing_data_.device = nullptr;
    tx_ack_wait_ = false;
    tx_retry_cnt_ = 0;
}
const cmd_hex_t *WallPadComponent::get_writing_cmd()
{
    return writing_data_.cmd;
}
WallPadDevice *WallPadComponent::get_writing_device()
{
    return writing_data_.device;
}
unsigned long WallPadComponent::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}
unsigned long WallPadComponent::get_time()
{
    return millis();
}

ValidateCode WallPadComponent::validate_data(bool log)
{
    if (parser_.data().size() < rx_checksum_len_)
    {
        if (log) ESP_LOGW(TAG, "[Read] Size error: %s", to_hex_string(parser_.buffer()).c_str());
        return ERR_SIZE;
    }
    if (rx_prefix_.has_value() && parser_.parse_header() == false)
    {
        if (log) ESP_LOGW(TAG, "[Read] Prefix error: %s", to_hex_string(parser_.buffer()).c_str());
        return ERR_PREFIX;
    }
    if (rx_suffix_.has_value() && parser_.parse_footer() == false)
    {
        if (log) ESP_LOGW(TAG, "[Read] Suffix error: %s", to_hex_string(parser_.buffer()).c_str());
        return ERR_SUFFIX;
    }
    uint8_t crc = rx_checksum_ ? make_rx_checksum(parser_.data(rx_checksum_len_)) : 0;
    if (rx_checksum_ && crc != parser_.data()[parser_.data().size() - rx_checksum_len_])
    {
        if (log) ESP_LOGW(TAG, "[Read] Checksum error: %s", to_hex_string(parser_.buffer()).c_str());
        return ERR_CHECKSUM;
    }
    return ERR_NONE;
}
WallPadComponent::WallPadComponent(int baud, num_t data, num_t parity, num_t stop, num_t rx_wait)
{
    conf_baud_ = baud;
    conf_data_ = data;
    conf_parity_ = parity;
    conf_stop_ = stop;
    conf_rx_wait_ = rx_wait;
}
void WallPadComponent::set_rx_prefix(std::vector<uint8_t> prefix)
{
    rx_prefix_ = prefix;
}
void WallPadComponent::set_rx_suffix(std::vector<uint8_t> suffix)
{
    rx_suffix_ = suffix;
}
void WallPadComponent::set_tx_prefix(std::vector<uint8_t> prefix)
{
    tx_prefix_ = prefix;
}
void WallPadComponent::set_tx_suffix(std::vector<uint8_t> suffix)
{
    tx_suffix_ = suffix;
}
void WallPadComponent::set_rx_checksum(CheckSum checksum)
{
    rx_checksum_ = checksum;
}
void WallPadComponent::set_rx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f)
{
    rx_checksum_f_ = f;
    rx_checksum_ = CHECKSUM_CUSTOM;
}
void WallPadComponent::set_tx_checksum(CheckSum checksum)
{
    tx_checksum_ = checksum;
}
void WallPadComponent::set_tx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f)
{
    tx_checksum_f_ = f;
    tx_checksum_ = CHECKSUM_CUSTOM;
}
uint8_t WallPadComponent::make_rx_checksum(const std::vector<uint8_t> &data) const
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
            if (this->rx_prefix_.has_value())
            {
                for (uint8_t byte : this->rx_prefix_.value()) { crc += byte; }
            }
            for (uint8_t byte : data) { crc += byte; }
            break;
        case CHECKSUM_XOR:
            if (this->rx_prefix_.has_value())
            {
                for (uint8_t byte : this->rx_prefix_.value()) { crc ^= byte; }
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

uint8_t WallPadComponent::make_tx_checksum(const std::vector<uint8_t> &data) const
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
            if (this->tx_prefix_.has_value())
            {
                for (uint8_t byte : this->tx_prefix_.value()) { crc += byte; }
            }
            for (uint8_t byte : data) { crc += byte; }
            break;
        case CHECKSUM_XOR:
            if (this->tx_prefix_.has_value())
            {
                for (uint8_t byte : this->tx_prefix_.value()) { crc ^= byte; }
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

}  // namespace wallpad
}  // namespace esphome