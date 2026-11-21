#pragma once
#include "esphome/core/stub.h"
#include <deque>
namespace esphome { namespace uart {
struct UARTComponent { size_t get_rx_buffer_size() {return 256;} };
class UARTDevice {public: UARTComponent uart_parent_{}; UARTComponent *parent_{&uart_parent_}; std::deque<uint8_t> bytes; bool available() {return !bytes.empty();} bool read_byte(uint8_t *out) {if (bytes.empty()) return false; *out=bytes.front(); bytes.pop_front(); return true;} void write_byte(uint8_t) {} void write_array(const std::vector<uint8_t>&) {} void flush() {} };
}}
