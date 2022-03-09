#pragma once
#include <vector>
#include <queue>
#include <HardwareSerial.h>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "wallpad_device.h"
#include "parser.h"

#define BUFFER_SIZE 128
#define RX_ENABLE false
#define TX_ENABLE true

namespace esphome {
namespace wallpad {

enum Model {
    MODEL_CUSTOM = 0,
    MODEL_KOCOM,
    MODEL_SDS,
};

enum ValidateCode {
    ERR_NONE,
    ERR_SIZE,
    ERR_PREFIX,
    ERR_SUFFIX,
    ERR_CHECKSUM
};

enum CheckSum {
    CHECKSUM_NONE,
    CHECKSUM_CUSTOM,
    CHECKSUM_XOR,
    CHECKSUM_ADD
}

/** Send HEX Struct */
struct write_data
{
    WallPadDevice *device;
    const cmd_hex_t *cmd;
};

/** 
 * WallPad Core Component
 * 
 * @param baud Baud Rate
 * @param data Data bits
 * @param parity Parity(0: No parity, 2: Even, 3: Odd)
 * @param stop Stop bits
 * @param rx_wait RX Receive Timeout (mSec)
 */
class WallPadComponent : public Component
{
public:
    WallPadComponent(int baud, num_t data = 8, num_t parity = 0, num_t stop = 1, num_t rx_wait = 15);
    void set_rx_prefix(std::vector<uint8_t> prefix);
    void set_rx_suffix(std::vector<uint8_t> suffix);
    void set_tx_prefix(std::vector<uint8_t> prefix);
    void set_tx_suffix(std::vector<uint8_t> suffix);
    void set_rx_checksum(CheckSum checksum);
    void set_rx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f);
    void set_tx_checksum(CheckSum checksum);
    void set_tx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f);
    uint8_t make_rx_checksum(const std::vector<uint8_t> &data) const;
    uint8_t make_tx_checksum(const std::vector<uint8_t> &data) const;
    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS; }
    void write_byte(uint8_t data);
    void write_array(const std::vector<uint8_t> &data);
    void write_with_header(const std::vector<uint8_t> &data);
    void write_next(const write_data data);
    void write_next_late(const write_data data);
    void flush();
    void register_device(WallPadDevice *device);
    void set_tx_interval(num_t tx_interval);
    void set_tx_wait(num_t tx_wait);
    void set_tx_retry_cnt(num_t tx_retry_cnt);
    void set_ctrl_pin(InternalGPIOPin *pin);
    void set_status_pin(InternalGPIOPin *pin);
    void set_tx_pin(InternalGPIOPin *tx_pin);
    void set_rx_pin(InternalGPIOPin *rx_pin);
    void set_model(Model model);
    Model get_model();
    bool is_have_writing_data();
    void clear_writing_data();
    const cmd_hex_t* get_writing_cmd();
    WallPadDevice* get_writing_device();
    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
protected:
    HardwareSerial *hw_serial_{nullptr};
    std::vector<WallPadDevice *> devices_{};
    Model conf_model_;
    int conf_baud_;
    num_t conf_data_;
    num_t conf_parity_;
    num_t conf_stop_;
    num_t conf_rx_wait_;
    num_t conf_tx_interval_{50};
    num_t conf_tx_wait_{50};
    num_t conf_tx_retry_cnt_{3};

    optional<std::vector<uint8_t>> rx_prefix_{};
    optional<std::vector<uint8_t>> rx_suffix_{};
    optional<std::vector<uint8_t>> tx_prefix_{};
    optional<std::vector<uint8_t>> tx_suffix_{};

    num_t rx_checksum_len_{0};
    CheckSum rx_checksum_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len)>> rx_checksum_f_{};
 
    CheckSum tx_checksum_{CHECKSUM_NONE};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len)>> tx_checksum_f_{};

    ValidateCode validate_data(bool log = false);

    void read_from_serial();
    void treat_recived_data();
    bool validate_ack();
    void publish_data();

    void write_to_serial();
    bool retry_write();
    void write_command();
    void pop_command_to_write();

    unsigned long rx_lastTime_{0};
    std::queue<write_data> tx_queue_{};
    std::queue<write_data> tx_queue_late_{};

    write_data writing_data_{};
    unsigned long tx_start_time_{0};
    bool tx_ack_wait_{false};
    num_t tx_retry_cnt_{0};
    InternalGPIOPin *ctrl_pin_{nullptr};
    InternalGPIOPin *status_pin_{nullptr};
    InternalGPIOPin *tx_pin_{nullptr};
    InternalGPIOPin *rx_pin_{nullptr};
    Parser parser_{};
};

} // namespace wallpad
} // namespace esphome