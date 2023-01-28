#pragma once

namespace esphome {
namespace divoom {
#define VERSION "2.0.0-230128"
static const uint8_t DIVOOM_HEADER  = 0x01;
static const uint8_t DIVOOM_FOOTER  = 0x02;
static const uint32_t MAX_WIDTH      = 8 * 20;
static const uint32_t MAX_HEIGHT     = 16;
}  // namespace divoom
}  // namespace esphome
