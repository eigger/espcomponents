#include "checksum.h"

namespace esphome {
namespace uartex {

uint16_t compute_checksum(CHECKSUM checksum, const std::vector<uint8_t> &header, const std::vector<uint8_t> &data)
{
    uint16_t crc = 0;
    uint8_t temp = 0;
    switch (checksum)
    {
    case CHECKSUM_XOR:
        for (uint8_t byte : header) { crc ^= byte; }
        for (uint8_t byte : data) { crc ^= byte; }
        break;
    case CHECKSUM_ADD:
        for (uint8_t byte : header) { crc += byte; }
        for (uint8_t byte : data) { crc += byte; }
        break;
    case CHECKSUM_XOR_NO_HEADER:
        for (uint8_t byte : data) { crc ^= byte; }
        break;
    case CHECKSUM_ADD_NO_HEADER:
        for (uint8_t byte : data) { crc += byte; }
        break;
    case CHECKSUM_XOR_ADD:
        for (uint8_t byte : header)
        {
            crc += byte;
            temp ^= byte;
        }
        for (uint8_t byte : data)
        {
            crc += byte;
            temp ^= byte;
        }
        crc += temp;
        crc = ((uint16_t) temp << 8) | (crc & 0xFF);
        break;
    case CHECKSUM_NONE:
    case CHECKSUM_CUSTOM:
        break;
    }
    return crc;
}

}  // namespace uartex
}  // namespace esphome
