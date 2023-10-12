#pragma once
#include <vector>
#include <queue>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "uartex_device.h"
#include "parser.h"
#include "version.h"
namespace esphome {
namespace uartex {

enum ERROR {
    ERROR_NONE,
    ERROR_SIZE,
    ERROR_HEADER,
    ERROR_FOOTER,
    ERROR_CHECKSUM,
    ERROR_CHECKSUM_2,
    ERROR_ACK
};

enum CHECKSUM {
    CHECKSUM_NONE,
    CHECKSUM_CUSTOM,
    CHECKSUM_XOR,
    CHECKSUM_ADD
};

struct tx_data_t
{
    UARTExDevice* device;
    const cmd_t* cmd;
};

class UARTExComponent : public uart::UARTDevice, public Component
{
public:
    UARTExComponent() = default;
    void set_rx_header(std::vector<uint8_t> header);
    void set_rx_footer(std::vector<uint8_t> footer);
    void set_tx_header(std::vector<uint8_t> header);
    void set_tx_footer(std::vector<uint8_t> footer);
    void set_rx_checksum(CHECKSUM checksum);
    void set_rx_checksum_2(CHECKSUM checksum);
    void set_rx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f);
    void set_rx_checksum_2_lambda(std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)> &&f);
    void set_tx_checksum(CHECKSUM checksum);
    void set_tx_checksum_2(CHECKSUM checksum);
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
    void enqueue_tx_data(const tx_data_t data, bool low_priority = false);
    void write_flush(const unsigned long timer);
    void register_device(UARTExDevice *device);
    void set_tx_delay(uint16_t tx_delay);
    void set_tx_timeout(uint16_t timeout);
    void set_tx_retry_cnt(uint16_t tx_retry_cnt);
    void set_rx_timeout(uint16_t timeout);
    void set_tx_ctrl_pin(InternalGPIOPin *pin);
    bool is_have_tx_data();
    void ack_response(bool ok);
    void clear_tx_data();
    const cmd_t* tx_cmd();
    UARTExDevice* tx_device();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();

    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_error(text_sensor::TextSensor *error) { error_ = error; }
protected:

    std::vector<UARTExDevice *> devices_{};
    uint16_t conf_rx_timeout_{10};
    uint16_t conf_tx_delay_{50};
    uint16_t conf_tx_timeout_{50};
    uint16_t conf_tx_retry_cnt_{3};

    optional<std::vector<uint8_t>> rx_header_{};
    optional<std::vector<uint8_t>> rx_footer_{};
    optional<std::vector<uint8_t>> tx_header_{};
    optional<std::vector<uint8_t>> tx_footer_{};

    CHECKSUM rx_checksum_{CHECKSUM_NONE};
    CHECKSUM rx_checksum_2_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len)>> rx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)>> rx_checksum_f_2_{};
 
    CHECKSUM tx_checksum_{CHECKSUM_NONE};
    CHECKSUM tx_checksum_2_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len)>> tx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t checksum)>> tx_checksum_f_2_{};

    ERROR validate_data();
    bool publish_error(ERROR error_code);
    ERROR error_code_{ERROR_NONE};
    void read_from_uart();
    void publish_to_devices();
    bool verify_ack();
    void publish_data();

    void write_to_uart();
    bool retry_tx_data();
    void write_tx_data();
    void dequeue_tx_data_from_devices();

    unsigned long rx_time_{0};
    std::queue<tx_data_t> tx_queue_{};
    std::queue<tx_data_t> tx_queue_low_priority_{};

    tx_data_t tx_data_{nullptr, nullptr};
    unsigned long tx_time_{0};
    uint16_t tx_retry_cnt_{0};
    InternalGPIOPin *tx_ctrl_pin_{nullptr};
    Parser rx_parser_{};

    text_sensor::TextSensor *version_{nullptr};
    text_sensor::TextSensor *error_{nullptr};
};

} // namespace uartex
} // namespace esphome