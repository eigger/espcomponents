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
namespace wallpad {


enum Model {
    MODEL_CUSTOM = 0,
    MODEL_KOCOM,
    MODEL_SDS,
};

typedef unsigned short num_t;

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
    num_t length;    // 1~4
    num_t precision; // 0~5
};

/** Command HEX Struct */
struct cmd_hex_t
{
    std::vector<uint8_t> data;
    std::vector<uint8_t> ack;
};

}
}