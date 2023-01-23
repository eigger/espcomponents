#pragma once
#include <vector>
#include <queue>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "bluetoothex_device.h"
#include "BluetoothSerial.h"
#include "parser.h"
#include "version.h"
namespace esphome {
namespace bluetoothex {

enum ValidateCode {
    ERR_NONE,
    ERR_SIZE,
    ERR_HEADER,
    ERR_FOOTER,
    ERR_CHECKSUM,
    ERR_CHECKSUM_2,
    ERR_ACK
};

enum Checksum {
    CHECKSUM_NONE,
    CHECKSUM_CUSTOM,
    CHECKSUM_XOR,
    CHECKSUM_ADD
};

struct tx_data
{
    BluetoothExDevice* device;
    const cmd_t* cmd;
};

class BluetoothExComponent : public Component
{
public:
    BluetoothExComponent() = default;
    void set_address(uint64_t address);
    void set_device_name(std::string name) { device_name_ = name; }
    void set_rx_header(std::vector<uint8_t> header);
    void set_rx_footer(std::vector<uint8_t> footer);
    void set_tx_header(std::vector<uint8_t> header);
    void set_tx_footer(std::vector<uint8_t> footer);
    void set_rx_checksum(Checksum checksum);
    void set_rx_checksum_2(Checksum checksum);
    void set_rx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f);
    void set_rx_checksum_2_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)> &&f);
    void set_tx_checksum(Checksum checksum);
    void set_tx_checksum_2(Checksum checksum);
    void set_tx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f);
    void set_tx_checksum_2_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)> &&f);
    uint8_t get_rx_checksum(const std::vector<uint8_t> &data) const;
    uint8_t get_tx_checksum(const std::vector<uint8_t> &data) const;
    uint8_t get_rx_checksum_2(const std::vector<uint8_t> &data) const;
    uint8_t get_tx_checksum_2(const std::vector<uint8_t> &data) const;
    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }
    void write_data(const uint8_t data);
    void write_data(const std::vector<uint8_t> &data);
    void write_tx_cmd();
    void push_tx_data(const tx_data data);
    void push_tx_data_late(const tx_data data);
    void write_flush(const unsigned long timer);
    void register_device(BluetoothExDevice *device);
    void set_tx_delay(uint16_t tx_delay);
    void set_tx_timeout(uint16_t timeout);
    void set_tx_retry_cnt(uint16_t tx_retry_cnt);
    void set_rx_timeout(uint16_t timeout);
    void set_tx_ctrl_pin(InternalGPIOPin *pin);
    bool is_have_tx_cmd();
    void ack_tx_data(bool ok);
    void clear_tx_data();
    const cmd_t* tx_cmd();
    BluetoothExDevice* tx_device();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();

    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_error(text_sensor::TextSensor *error) { error_ = error; }
protected:
    BluetoothSerial serialbt_;
    bool connected_{false};
    std::vector<BluetoothExDevice *> devices_{};
    uint16_t conf_rx_timeout_{10};
    uint16_t conf_tx_delay_{50};
    uint16_t conf_tx_timeout_{50};
    uint16_t conf_tx_retry_cnt_{3};

    optional<std::string> device_name_{};
    uint8_t address_[6];
    optional<std::vector<uint8_t>> rx_header_{};
    optional<std::vector<uint8_t>> rx_footer_{};
    optional<std::vector<uint8_t>> tx_header_{};
    optional<std::vector<uint8_t>> tx_footer_{};

    Checksum rx_checksum_{CHECKSUM_NONE};
    Checksum rx_checksum_2_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len)>> rx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)>> rx_checksum_f_2_{};
 
    Checksum tx_checksum_{CHECKSUM_NONE};
    Checksum tx_checksum_2_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len)>> tx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)>> tx_checksum_f_2_{};

    ValidateCode validate_data();
    bool publish_error(ValidateCode error_code);
    ValidateCode error_code_{ValidateCode::ERR_NONE};
    void connect_to_device();
    void read_from_bluetooth();
    void publish_to_devices();
    bool validate_ack();
    void publish_data();

    void write_to_bluetooth();
    bool retry_tx_cmd();
    void write_tx_data();
    void pop_tx_data();

    unsigned long rx_time_{0};
    unsigned long disconnected_time_{0};
    std::queue<tx_data> tx_queue_{};
    std::queue<tx_data> tx_queue_late_{};

    tx_data tx_data_{nullptr, nullptr};
    unsigned long tx_time_{0};
    uint16_t tx_retry_cnt_{0};
    InternalGPIOPin *tx_ctrl_pin_{nullptr};
    Parser rx_parser_{};

    text_sensor::TextSensor *version_{nullptr};
    text_sensor::TextSensor *error_{nullptr};
};

} // namespace bluetoothex
} // namespace esphome