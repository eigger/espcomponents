#pragma once
#include <memory>
#include <deque>
#include "esphome/core/automation.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "uartex_device.h"
#include "parser.h"
#include "checksum.h"
namespace esphome {
namespace uartex {

enum ERROR {
    ERROR_NONE,
    ERROR_SIZE,
    ERROR_HEADER,
    ERROR_FOOTER,
    ERROR_CHECKSUM,
    ERROR_RX_TIMEOUT,
    ERROR_TX_TIMEOUT
};

enum PRIORITY {
    PRIORITY_DATA,
    PRIORITY_LOOP
};

struct tx_data_t
{
    UARTExDevice* device{nullptr};
    const cmd_t* cmd{nullptr};
    // Own snapshots through queueing, transmission, and retries.
    std::shared_ptr<const cmd_t> owned{};
    std::vector<uint8_t> copy_source{};
    // Queued by an entity control path (not polling): its transmission arms the state debounce bypass.
    bool control{false};
};

struct header_t
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> mask;
};

struct rx_data_length_t
{
    uint8_t offset{0};
    uint8_t length{1};
    ENDIAN endian{ENDIAN_BIG};
    int16_t adjust{0};
};

struct rx_reply_t
{
    state_t state;
    cmd_t command;
    optional<std::function<cmd_t(const uint8_t *data, const uint16_t len)>> command_f{};
};

class UARTExComponent : public uart::UARTDevice, public Component
{
public:
    UARTExComponent() = default;
    void add_rx_header(header_t header);
    void add_rx_reply(state_t state, cmd_t command);
    void add_rx_reply(state_t state, std::function<cmd_t(const uint8_t *data, const uint16_t len)> &&command_f);
    void set_rx_footer(std::vector<uint8_t> footer);
    void set_tx_header(std::vector<uint8_t> header);
    void set_tx_footer(std::vector<uint8_t> footer);
    void set_rx_checksum(CHECKSUM checksum);
    void set_rx_checksum(std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t *header, const uint16_t header_len)> &&f);
    void set_tx_checksum(CHECKSUM checksum);
    void set_tx_checksum(std::function<uint8_t(const uint8_t *data, const uint16_t len)> &&f);
    void set_rx_checksum_2(CHECKSUM checksum);
    void set_tx_checksum_2(CHECKSUM checksum);
    void set_rx_checksum_2(std::function<std::vector<uint8_t>(const uint8_t *data, const uint16_t len)> &&f);
    void set_tx_checksum_2(std::function<std::vector<uint8_t>(const uint8_t *data, const uint16_t len)> &&f);
    void set_rx_priority(PRIORITY priority);
    void set_version(text_sensor::TextSensor *version) { this->version_ = version; }
    void set_version_str(const char *version_str) { this->version_str_ = version_str; }
    void set_error(text_sensor::TextSensor *error) { this->error_ = error; }
    void set_log(text_sensor::TextSensor *log) { this->log_ = log; }
    void set_log_ascii(bool ascii) { this->log_ascii_ = ascii; }
    void add_on_write_callback(std::function<void(const uint8_t *data, const uint16_t len)> &&callback) { this->write_callback_.add(std::move(callback)); }
    void add_on_read_callback(std::function<void(const uint8_t *data, const uint16_t len)> &&callback) { this->read_callback_.add(std::move(callback)); }
    void add_on_error_callback(std::function<void(const ERROR)> &&callback) { this->error_callback_.add(std::move(callback)); }
    std::vector<uint8_t> get_rx_checksum(const std::vector<uint8_t> &data, const std::vector<uint8_t> &header);
    std::vector<uint8_t> get_tx_checksum(const std::vector<uint8_t> &data);
    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }
    void write_data(const uint8_t data);
    void write_data(const std::vector<uint8_t> &data);
    void write_flush();
    void register_device(UARTExDevice *device);
    void set_tx_delay(uint16_t tx_delay);
    void set_tx_timeout(uint16_t timeout);
    void set_tx_retry_cnt(uint16_t tx_retry_cnt);
    void set_tx_command_queue_size(uint16_t size);
    void set_rx_length(uint16_t rx_length);
    void set_rx_length(std::function<int16_t(const uint8_t *data, uint16_t len)> &&length);
    void set_rx_data_length(uint8_t offset, uint8_t length, ENDIAN endian, int16_t adjust);
    void set_rx_timeout(uint16_t timeout);
    void set_tx_ctrl_pin(InternalGPIOPin *pin);
    void enqueue_tx_data(const tx_data_t data, bool low_priority = false);
    bool enqueue_tx_reply(const tx_data_t data);
    void write_command(cmd_t cmd);
    void write_immediate_reply(const std::vector<uint8_t> &data);
protected:
    bool is_tx_cmd_pending();
    void tx_cmd_result(bool result);
    void clear_tx_data();
    const cmd_t* current_tx_cmd();
    void write_tx_cmd();
    std::vector<uint8_t> transmit_frame(const std::vector<uint8_t> &data);
    void publish_tx_frame(const std::vector<uint8_t> &frame);
    ERROR validate_data();
    bool verify_data();
    bool publish_error(ERROR error_code);
    void publish_rx_log(const std::vector<unsigned char>& data);
    void publish_tx_log(const std::vector<unsigned char>& data);
    void publish_log(std::string msg);
    bool read_from_uart();
    bool rx_available();
    bool read_rx_byte(uint8_t &byte);
    bool parse_bytes();
    bool parse_templated_length();
    void publish_to_devices();
    bool verify_ack();
    void process_rx_reply();
    void publish_data();
    void write_to_uart();
    bool retry_tx_data();
    void write_tx_data();
    bool select_ready_tx(std::deque<tx_data_t> &queue);
    void dequeue_tx_data_from_devices();
protected:
    std::vector<UARTExDevice *> devices_{};
    uint16_t conf_rx_timeout_{10};
    uint16_t conf_tx_delay_{50};
    uint16_t conf_tx_timeout_{50};
    uint16_t conf_tx_retry_cnt_{3};
    uint16_t conf_tx_command_queue_size_{10};
    uint16_t conf_rx_length_{0};
    std::function<int16_t(const uint8_t *data, uint16_t len)> rx_length_f_{};
    optional<rx_data_length_t> conf_rx_data_length_{};
    std::vector<header_t> rx_headers_{};
    std::vector<rx_reply_t> rx_reply_{};
    optional<std::vector<uint8_t>> rx_footer_{};
    optional<std::vector<uint8_t>> tx_header_{};
    optional<std::vector<uint8_t>> tx_footer_{};
    PRIORITY rx_priority_{PRIORITY_DATA};
    CHECKSUM rx_checksum_{CHECKSUM_NONE};
    CHECKSUM tx_checksum_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len, const uint8_t *header, const uint16_t header_len)>> rx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const uint16_t len)>> tx_checksum_f_{};
    CHECKSUM rx_checksum_2_{CHECKSUM_NONE};
    CHECKSUM tx_checksum_2_{CHECKSUM_NONE};
    optional<std::function<std::vector<uint8_t>(const uint8_t *data, const uint16_t len)>> rx_checksum_f_2_{};
    optional<std::function<std::vector<uint8_t>(const uint8_t *data, const uint16_t len)>> tx_checksum_f_2_{};
    CallbackManager<void(const uint8_t *data, const uint16_t len)> write_callback_{};
    CallbackManager<void(const uint8_t *data, const uint16_t len)> read_callback_{};
    ERROR error_code_{ERROR_NONE};
    CallbackManager<void(const ERROR)> error_callback_{};
    std::deque<tx_data_t> tx_queue_{};
    // Bound reply backlog independently of the deprecated command queue setting.
    static constexpr size_t MAX_QUEUED_REPLIES = 10;
    std::queue<tx_data_t> tx_queue_reply_{};
    std::deque<tx_data_t> tx_queue_low_priority_{};
    tx_data_t current_tx_data_{nullptr, nullptr};
    bool rx_processing_{false};
    std::vector<uint8_t> rx_replay_{};
    unsigned long rx_timer_{0};
    unsigned long rx_time_{0};
    unsigned long tx_time_{0};
    unsigned long tx_command_time_{0};
    uint16_t tx_retry_cnt_{0};

    InternalGPIOPin *tx_ctrl_pin_{nullptr};
    Parser rx_parser_{};
    text_sensor::TextSensor* version_{nullptr};
    const char *version_str_{"unknown"};
    text_sensor::TextSensor* error_{nullptr};
    text_sensor::TextSensor* log_{nullptr};
    bool log_ascii_{false};
    std::string last_log_{""};
    uint32_t log_count_{0};
};

} // namespace uartex
} // namespace esphome
