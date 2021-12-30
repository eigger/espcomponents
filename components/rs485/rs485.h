#pragma once

#include <HardwareSerial.h>
#include <vector>
#include <queue>
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"

#define BUFFER_SIZE 128
#define RX_ENABLE false
#define TX_ENABLE true

namespace esphome {
namespace rs485 {

typedef unsigned short num_t;
class RS485Component;
class RS485Device;
class RS485Listener;

/** State HEX Struct */
struct hex_t
{
    num_t offset;
    bool and_operator;
    bool inverted;
    std::vector<uint8_t> data;
};

/** Number state HEX Struct  **/
struct state_num_t
{
    num_t offset;
    num_t length; // 1~4
    num_t precision; // 0~5
};

/** Command HEX Struct */
struct cmd_hex_t
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> ack;
};

/** Send HEX Struct */
struct send_hex_t
{
    RS485Device *device;
    const cmd_hex_t *cmd;
};


/**
 * RS485 Listener
 * @desc 각 컴포넌트에 수신 메시지 전달
 */
class RS485Listener {
    public:
        virtual bool parse_data(const uint8_t *data, const num_t len) = 0;
        void set_parent(RS485Component *parent) { parent_ = parent; }

        void set_monitor(bool monitor) { monitor_ = monitor; }
        bool is_monitor() { return monitor_; }

    protected:
        RS485Component *parent_{nullptr};
        bool monitor_{false};

};


/**
 * RS485 Device
 */
class RS485Device : public RS485Listener, public PollingComponent {
    public:
        void update() override;
        void dump_rs485_device_config(const char *TAG);

        void set_device(hex_t device) { device_ = device; }
        void set_sub_device(hex_t sub_device) { sub_device_ = sub_device; }
        void set_state_on(hex_t state_on) { state_on_ = state_on; }
        void set_state_off(hex_t state_off) { state_off_ = state_off; }
        
        void set_command_on(cmd_hex_t command_on) { command_on_ = command_on; }
        void set_command_on(std::function<cmd_hex_t()> command_on_func) { command_on_func_ = command_on_func; }
        const cmd_hex_t* get_command_on() { if(command_on_func_.has_value()) command_on_ = (*command_on_func_)(); return &command_on_.value(); }
        
        void set_command_off(cmd_hex_t command_off) { command_off_ = command_off; }
        void set_command_off(std::function<cmd_hex_t()> command_off_func) { command_off_func_ = command_off_func; }
        const cmd_hex_t* get_command_off() { if(command_off_func_.has_value()) command_off_ = (*command_off_func_)(); return &command_off_.value(); }

        void set_command_state(cmd_hex_t command_state) { command_state_ = command_state; }

        void write_with_header(const cmd_hex_t *cmd);
        void callback() { tx_pending_ = false; }
        
        /** RS485 raw message parse */
        bool parse_data(const uint8_t *data, const num_t len) override;

        /** Publish other message from parse_date() */
        virtual void publish(const uint8_t *data, const num_t len) = 0;

        /** Publish on/off state message from parse_date() */
        virtual bool publish(bool state) = 0;

        /** priority of setup(). higher -> executed earlier */
        float get_setup_priority() const override { return setup_priority::DATA; }


    protected:
        const std::string *device_name_;
        hex_t device_{};
        optional<hex_t> sub_device_{};
        optional<hex_t> state_on_{};
        optional<hex_t> state_off_{};
        optional<cmd_hex_t> command_on_{};
        optional<std::function<cmd_hex_t()>> command_on_func_{};
        optional<cmd_hex_t> command_off_{};
        optional<std::function<cmd_hex_t()>> command_off_func_{};
        optional<cmd_hex_t> command_state_;

        bool tx_pending_{false};



};


/** 
 * RS485 Core Component
 * 
 * @param baud Baud Rate
 * @param data Data bits
 * @param parity Parity(0: No parity, 2: Even, 3: Odd)
 * @param stop Stop bits
 * @param rx_wait RX Receive Timeout (mSec)
 */
class RS485Component : public Component {
    public:
        RS485Component(int baud, num_t data=8, num_t parity=0, num_t stop=1, num_t rx_wait=15) {
            conf_baud_   = baud;
            conf_data_   = data;
            conf_parity_ = parity;
            conf_stop_   = stop;
            conf_rx_wait_ = rx_wait;
        }

        /** 시작부(수신시 Check, 발신시 Append) */
        void set_prefix(std::vector<uint8_t> prefix) { prefix_ = prefix; prefix_len_ = prefix.size(); }

        /** 종료부(수신시 Check, 발신시 Append) */
        void set_suffix(std::vector<uint8_t> suffix) { suffix_ = suffix; suffix_len_ = suffix.size(); }

        /** CheckSum8 Xor 사용 여부 (수신시 Check, 발신시 Append) */
        void set_checksum(bool checksum) { checksum_ = checksum; } // xor sum
        void set_checksum_lambda(std::function<uint8_t(const uint8_t *data, const num_t len)> &&f) { checksum_f_ = f; checksum_ = true; }

        /** CheckSum8 Add 사용 여부 (수신시 Check, 발신시 Append) */
        void set_checksum2(bool checksum2) { checksum2_ = checksum2; } // add sum
        void set_checksum2_lambda(std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum1)> &&f) { checksum2_f_ = f; checksum2_ = true; }

        /** CheckSum Calc */
        uint8_t make_checksum(const uint8_t *data, const num_t len) const;
        uint8_t make_checksum2(const uint8_t *data, const num_t len, const uint8_t checksum1) const;

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
        void write_next_late(const cmd_hex_t *cmd);
        void flush();

        void register_listener(RS485Listener *listener) {
            listener->set_parent(this);
            this->listeners_.push_back(listener);
        }

        /** TX interval time */
        void set_tx_interval(num_t tx_interval) { conf_tx_interval_ = tx_interval; }

        /** TX Ack wait time */
        void set_tx_wait(num_t tx_wait) { conf_tx_wait_ = tx_wait; }

        /** TX Retry count */
        void set_tx_retry_cnt(num_t tx_retry_cnt) { conf_tx_retry_cnt_ = tx_retry_cnt; }

        /** RX,TX Control pin */
        void set_ctrl_pin(GPIOPin *pin) { ctrl_pin_ = pin; }

        /** Response Packet Pattern */
        void set_state_response(hex_t state_response) { state_response_ = state_response; }

    protected:
        HardwareSerial *hw_serial_{nullptr};
        std::vector<RS485Listener *> listeners_{};

        int conf_baud_;
        num_t conf_data_;
        num_t conf_parity_;
        num_t conf_stop_;
        num_t conf_rx_wait_;
        num_t conf_tx_interval_{50};
        num_t conf_tx_wait_{50};
        num_t conf_tx_retry_cnt_{3};
        optional<hex_t> state_response_{};

        optional<std::vector<uint8_t>> prefix_{};
        num_t prefix_len_{0};
        optional<std::vector<uint8_t>> suffix_{};
        num_t suffix_len_{0};
        num_t checksum_len_{0};
        bool checksum_{false};
        bool checksum2_{false};
        optional<std::function<uint8_t(const uint8_t *data, const num_t len)>> checksum_f_{};
        optional<std::function<uint8_t(const uint8_t *data, const num_t len, const uint8_t checksum1)>> checksum2_f_{};

        /** 수신데이터 검증 */
        bool validate(const uint8_t *data, const num_t len);
        /** 수신처리 */
        void rx_proc();
        /** 전송처리 */
        void tx_proc();

        /** 초기화 여부 */
        bool init_{false};
        /** 응답 대기 상태 여부 */
        bool response_wait_{false};
        
        //////// 수신처리 관련 변수  ////////
        uint8_t rx_buffer_[BUFFER_SIZE]{};
        int     rx_timeOut_{conf_rx_wait_};
        num_t   rx_bytesRead_{0};
        unsigned long rx_lastTime_{0};

        /** queue for Command */
        std::queue<send_hex_t> tx_queue_{};
        /** queue for State request */
        std::queue<const cmd_hex_t*> tx_queue_late_{};

        //////// 전송처리 관련 변수  ////////
        const cmd_hex_t *tx_current_cmd_{nullptr};
        RS485Device *tx_current_device_{nullptr};
        unsigned long tx_start_time_{0};
        bool tx_ack_wait_{false};
        num_t tx_retry_cnt_{0};
        GPIOPin *ctrl_pin_{nullptr};

};

/** uint8_t[] to hex string  */
std::string hexencode(const uint8_t *raw_data, const num_t len);

/** uint8_t[] compare */
bool compare(const uint8_t *data1, const num_t len1, const uint8_t *data2, const num_t len2, const num_t offset);
bool compare(const uint8_t *data1, const num_t len1, const hex_t *data2);

/** uint8_t[] to decimal(float) */
float hex_to_float(const uint8_t *data, const num_t len, const num_t precision);

/** 패킷 캡쳐용 리스너 */
class SerialMonitor : public RS485Listener {
    public:
        SerialMonitor(hex_t filter = {}) { if(!filter.data.empty()) filters_.push_back(filter); set_monitor(true); }
        void add_filter(hex_t filter) { filters_.push_back(filter); }
        bool parse_data(const uint8_t *data, const num_t len) override;

    protected:
        std::vector<hex_t> filters_;


};

}  // namespace rs485
}  // namespace esphome