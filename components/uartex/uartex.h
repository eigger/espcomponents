#pragma once
#include <vector>
#include <queue>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"
#include "uartex_device.h"
#include "parser.h"


namespace esphome {
namespace uartex {
#define UARTEX_VERSION "1.0.0.0-220914"
enum ValidateCode {
    ERR_NONE,
    ERR_SIZE,
    ERR_PREFIX,
    ERR_SUFFIX,
    ERR_CHECKSUM
};

enum Checksum {
    CHECKSUM_NONE,
    CHECKSUM_CUSTOM,
    CHECKSUM_XOR,
    CHECKSUM_ADD
};

struct tx_data
{
    UARTExDevice* device;
    const cmd_t* cmd;
};

class UARTExComponent : public text_sensor::TextSensor, public uart::UARTDevice, public Component
{
public:
    UARTExComponent() = default;
    void set_rx_prefix(std::vector<uint8_t> prefix);
    void set_rx_suffix(std::vector<uint8_t> suffix);
    void set_tx_prefix(std::vector<uint8_t> prefix);
    void set_tx_suffix(std::vector<uint8_t> suffix);
    void set_rx_checksum(Checksum checksum);
    void set_rx_checksum_2(Checksum checksum);
    void set_rx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f);
    void set_rx_checksum_2_lambda(std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum)> &&f);
    void set_tx_checksum(Checksum checksum);
    void set_tx_checksum_2(Checksum checksum);
    void set_tx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f);
    void set_tx_checksum_2_lambda(std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum)> &&f);
    uint8_t get_rx_checksum(const std::vector<uint8_t> &data) const;
    uint8_t get_tx_checksum(const std::vector<uint8_t> &data) const;
    uint8_t get_rx_checksum_2(const std::vector<uint8_t> &data) const;
    uint8_t get_tx_checksum_2(const std::vector<uint8_t> &data) const;
    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }
    std::string unique_id() override;
    void write_data(const uint8_t data);
    void write_data(const std::vector<uint8_t> &data);
    void write_tx_cmd();
    void push_tx_data(const tx_data data);
    void push_tx_data_late(const tx_data data);
    void flush();
    void register_device(UARTExDevice *device);
    void set_tx_interval(num_t tx_interval);
    void set_tx_wait(num_t tx_wait);
    void set_tx_retry_cnt(num_t tx_retry_cnt);
    void set_rx_wait(num_t rx_wait);
    void set_ctrl_pin(InternalGPIOPin *pin);
    void set_status_pin(InternalGPIOPin *pin);
    bool is_have_tx_cmd();
    void ack_tx_data(bool ok);
    void clear_tx_data();
    const cmd_t* tx_cmd();
    UARTExDevice* tx_device();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
protected:

    std::vector<UARTExDevice *> devices_{};
    num_t conf_rx_wait_;
    num_t conf_tx_interval_{50};
    num_t conf_tx_wait_{50};
    num_t conf_tx_retry_cnt_{3};

    optional<std::vector<uint8_t>> rx_prefix_{};
    optional<std::vector<uint8_t>> rx_suffix_{};
    optional<std::vector<uint8_t>> tx_prefix_{};
    optional<std::vector<uint8_t>> tx_suffix_{};

    Checksum rx_checksum_{CHECKSUM_NONE};
    Checksum rx_checksum_2_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len)>> rx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum)>> rx_checksum_f_2_{};
 
    Checksum tx_checksum_{CHECKSUM_NONE};
    Checksum tx_checksum_2_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len)>> tx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum)>> tx_checksum_f_2_{};
    ValidateCode validate_data(bool log = false);

    void read_from_uart();
    void publish_to_devices();
    bool validate_ack();
    void publish_data();

    void write_to_uart();
    bool retry_tx_cmd();
    void write_tx_data();
    void pop_tx_data();

    unsigned long rx_time_{0};
    std::queue<tx_data> tx_queue_{};
    std::queue<tx_data> tx_queue_late_{};

    tx_data tx_data_{nullptr, nullptr};
    unsigned long tx_time_{0};
    num_t tx_retry_cnt_{0};
    InternalGPIOPin *ctrl_pin_{nullptr};
    InternalGPIOPin *status_pin_{nullptr};
    Parser rx_parser_{};
};

} // namespace uartex
} // namespace esphome