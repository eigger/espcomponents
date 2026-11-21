#include "uartex_device.h"
#include <cassert>
#include <cmath>
#include <vector>
using namespace esphome::uartex;

int main() {
  for (uint16_t length = 1; length <= 16; ++length) {
    std::vector<uint8_t> negative_one(length, 0xFF);
    assert(state_to_float(negative_one, state_num_t(0, length, 0, true, ENDIAN_BIG)) == -1);
    assert(state_to_float(negative_one, state_num_t(0, length, 0, true, ENDIAN_LITTLE)) == -1);
    std::vector<uint8_t> one(length, 0); one.back() = 1;
    assert(state_to_float(one, state_num_t(0, length, 0, false, ENDIAN_BIG)) == 1);
    one.back() = 0; one.front() = 1;
    assert(state_to_float(one, state_num_t(0, length, 0, false, ENDIAN_LITTLE)) == 1);
  }
  assert(state_to_float({0xFF}, state_num_t(0, 1, 0, true)) == -1);
  assert(state_to_float({0x80, 0x00}, state_num_t(0, 2, 0, true)) == -32768);
  assert(state_to_float({0x00, 0x80}, state_num_t(0, 2, 0, true, ENDIAN_LITTLE)) == -32768);
  assert(state_to_float({0xFF, 0xFF, 0xFF, 0xFF}, state_num_t(0, 4, 0, true)) == -1);
  assert(state_to_float({1, 2, 3, 4, 5}, state_num_t(0, 5, 0, false)) == static_cast<float>(0x0102030405ULL));
  assert(state_to_float({5, 4, 3, 2, 1}, state_num_t(0, 5, 0, false, ENDIAN_LITTLE)) == static_cast<float>(0x0102030405ULL));
  assert(state_to_float({0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, state_num_t(0, 5, 0, true)) == -1);
  assert(state_to_float({0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, state_num_t(0, 5, 0, true, ENDIAN_LITTLE)) == -1);
  assert(state_to_float(std::vector<uint8_t>(16, 0xFF), state_num_t(0, 16, 0, true)) == -1);
  assert(std::isinf(state_to_float(std::vector<uint8_t>(16, 0xFF), state_num_t(0, 16, 0, false))));
  assert(std::isfinite(state_to_float(std::vector<uint8_t>(16, 0xFF), state_num_t(0, 16, 1, false))));
  assert(state_to_float({0x12, 0x34, 0x56, 0x78, 0x90}, state_num_t(0, 5, 0, false, ENDIAN_BIG, DECODE_BCD)) == static_cast<float>(1234567890));
  assert(state_to_float(std::vector<uint8_t>(16, 0x99), state_num_t(0, 16, 0, false, ENDIAN_BIG, DECODE_BCD)) > 1e31f);
  assert(state_to_float({0x12, 0x34, 0xFA, 0x56}, state_num_t(0, 4, 0, false, ENDIAN_BIG, DECODE_BCD)) == 1234);
  assert(state_to_float({'1', '2', '3', '4', '5'}, state_num_t(0, 5, 1, true, ENDIAN_BIG, DECODE_ASCII)) == 1234.5f);
  assert(state_to_float({0xFF}, state_num_t(0, 2, 0, true)) == 255); // Missing high-order bytes remain zero.
  assert(state_to_float({}, state_num_t(0, 1, 0, true)) == 0);
}
