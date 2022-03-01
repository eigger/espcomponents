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
    ERR_CHECKSUM,
    ERR_CHECKSUM2
};


/** Send HEX Struct */
struct send_hex_t
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
    WallPadComponent(int baud, num_t data = 8, num_t parity = 0, num_t stop = 1, num_t rx_wait = 15)
    {
        conf_baud_ = baud;
        conf_data_ = data;
        conf_parity_ = parity;
        conf_stop_ = stop;
        conf_rx_wait_ = rx_wait;
    }

    /** 시작부(수신시 Check) */
    void set_rx_prefix(std::vector<uint8_t> prefix)
    {
        rx_prefix_ = prefix;
        rx_prefix_len_ = prefix.size();
    }

    /** 종료부(수신시 Check) */
    void set_rx_suffix(std::vector<uint8_t> suffix)
    {
        rx_suffix_ = suffix;
        rx_suffix_len_ = suffix.size();
    }

    /** 시작부(발신시 Append) */
    void set_tx_prefix(std::vector<uint8_t> prefix)
    {
        tx_prefix_ = prefix;
        tx_prefix_len_ = prefix.size();
    }

    /** 종료부(발신시 Append) */
    void set_tx_suffix(std::vector<uint8_t> suffix)
    {
        tx_suffix_ = suffix;
        tx_suffix_len_ = suffix.size();
    }

    /** CheckSum8 Xor 사용 여부 (수신시 Check) */
    void set_rx_checksum(bool checksum) { rx_checksum_ = checksum; } // xor sum
    void set_rx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f)
    {
        rx_checksum_f_ = f;
        rx_checksum_ = true;
    }

    /** CheckSum8 Add 사용 여부 (수신시 Check) */
    void set_rx_checksum2(bool checksum2) { rx_checksum2_ = checksum2; } // add sum
    void set_rx_checksum2_lambda(std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum1)> &&f)
    {
        rx_checksum2_f_ = f;
        rx_checksum2_ = true;
    }

    /** CheckSum8 Xor 사용 여부 (발신시 Append) */
    void set_tx_checksum(bool checksum) { tx_checksum_ = checksum; } // xor sum
    void set_tx_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f)
    {
        tx_checksum_f_ = f;
        tx_checksum_ = true;
    }

    /** CheckSum8 Add 사용 여부 (발신시 Append) */
    void set_tx_checksum2(bool checksum2) { tx_checksum2_ = checksum2; } // add sum
    void set_tx_checksum2_lambda(std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum1)> &&f)
    {
        tx_checksum2_f_ = f;
        tx_checksum2_ = true;
    }

    /** CheckSum Calc */
    uint8_t make_rx_checksum(const uint8_t *data, const num_t len) const;
    uint8_t make_rx_checksum2(const uint8_t *data, const num_t len, const uint8_t checksum1) const;
    uint8_t make_tx_checksum(const uint8_t *data, const num_t len) const;
    uint8_t make_tx_checksum2(const uint8_t *data, const num_t len, const uint8_t checksum1) const;

    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS; }

    void write_byte(uint8_t data);
    void write_array(const uint8_t *data, const num_t len);
    void write_array(const std::vector<uint8_t> &data) { this->write_array(&data[0], data.size()); }
    void write_with_header(const std::vector<uint8_t> &data);
    /** write for Command */
    void write_next(const send_hex_t send);
    /** write for State request */
    void write_next_late(const send_hex_t send);
    void flush();

    void register_device(WallPadDevice *device) { devices_.push_back(device); }

    /** TX interval time */
    void set_tx_interval(num_t tx_interval) { conf_tx_interval_ = tx_interval; }

    /** TX Ack wait time */
    void set_tx_wait(num_t tx_wait) { conf_tx_wait_ = tx_wait; }

    /** TX Retry count */
    void set_tx_retry_cnt(num_t tx_retry_cnt) { conf_tx_retry_cnt_ = tx_retry_cnt; }

    /** RX,TX Control pin */
    void set_ctrl_pin(InternalGPIOPin *pin) { ctrl_pin_ = pin; }

    void set_tx_pin(InternalGPIOPin *tx_pin) { tx_pin_ = tx_pin; }
    void set_rx_pin(InternalGPIOPin *rx_pin) { rx_pin_ = rx_pin; }

    void set_model(Model model) { conf_model_ = model;}
    Model get_model() { return conf_model_; }

    bool is_send_cmd() { if (tx_send_cmd_.device) return true; return false; }
    void clear_send_cmd() { tx_send_cmd_.device = nullptr; tx_ack_wait_ = false; tx_retry_cnt_ = 0; }
    const cmd_hex_t* get_send_cmd() { return tx_send_cmd_.cmd; }
    WallPadDevice* get_send_device() { return tx_send_cmd_.device; }
    unsigned long elapsed_time(const unsigned long timer) { return millis() - timer; }
    unsigned long get_time() { return millis(); }
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
    num_t rx_prefix_len_{0};
    optional<std::vector<uint8_t>> rx_suffix_{};
    num_t rx_suffix_len_{0};
    optional<std::vector<uint8_t>> tx_prefix_{};
    num_t tx_prefix_len_{0};
    optional<std::vector<uint8_t>> tx_suffix_{};
    num_t tx_suffix_len_{0};

    num_t rx_checksum_len_{0};
    bool rx_checksum_{false};
    bool rx_checksum2_{false};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len)>> rx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum1)>> rx_checksum2_f_{};

    num_t tx_checksum_len_{0};
    bool tx_checksum_{false};
    bool tx_checksum2_{false};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len)>> tx_checksum_f_{};
    optional<std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum1)>> tx_checksum2_f_{};

    ValidateCode validate_data(bool log = false);

    void recive_from_serial();
    void treat_recived_data();
    bool validate_ack();
    void publish_data();

    void send_to_serial();
    bool send_retry();
    void send_command();
    void pop_tx_command();

    unsigned long rx_lastTime_{0};

    /** queue for Command */
    std::queue<send_hex_t> tx_queue_{};
    /** queue for State request */
    std::queue<send_hex_t> tx_queue_late_{};

    send_hex_t tx_send_cmd_{};
    unsigned long tx_start_time_{0};
    bool tx_ack_wait_{false};
    num_t tx_retry_cnt_{0};
    InternalGPIOPin *ctrl_pin_{nullptr};
    InternalGPIOPin *tx_pin_{nullptr};
    InternalGPIOPin *rx_pin_{nullptr};
    Parser parser_{};
};

} // namespace wallpad
} // namespace esphome