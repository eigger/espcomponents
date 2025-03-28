#include "uartex_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.button";

void UARTExButton::dump_config()
{
    uartex_dump_config(TAG);
}

}  // namespace uartex
}  // namespace esphome
