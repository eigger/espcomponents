#pragma once
#include "define.h"

namespace esphome {
namespace wallpad {

/** uint8_t[] to hex string  */
std::string hexencode(const uint8_t *raw_data, const num_t len);

/** uint8_t[] compare */
bool compare(const uint8_t *data1, const num_t len1, const uint8_t *data2, const num_t len2, const num_t offset);
bool compare(const uint8_t *data1, const num_t len1, const hex_t *data2);

/** uint8_t[] to decimal(float) */
float hex_to_float(const uint8_t *data, const num_t len, const num_t precision);

}
}
