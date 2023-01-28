#pragma once

namespace esphome {
namespace divoom {
#define VERSION "1.0.0-230127"
static const uint8_t DIVOOM_HEADER  = 0x01;
static const uint8_t DIVOOM_FOOTER  = 0x02;
static const uint8_t MAX_WIDTH      = 8 * 50;
static const uint8_t MAX_HEIGHT     = 64;
}  // namespace divoom
}  // namespace esphome
