#pragma once
#include <cstdint>
#include <vector>

namespace esphome {
namespace uartex {

enum CHECKSUM {
    CHECKSUM_NONE,
    CHECKSUM_CUSTOM,
    CHECKSUM_XOR,
    CHECKSUM_ADD,
    CHECKSUM_XOR_NO_HEADER,
    CHECKSUM_ADD_NO_HEADER,
    CHECKSUM_XOR_ADD
};

uint16_t compute_checksum(CHECKSUM checksum, const std::vector<uint8_t> &header, const std::vector<uint8_t> &data);

}  // namespace uartex
}  // namespace esphome
