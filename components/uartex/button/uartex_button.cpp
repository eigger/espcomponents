#include "uartex_button.h"

namespace esphome {
namespace uartex {

static const char *TAG = "uartex.button";

void UARTExButton::dump_config()
{
#ifdef ESPHOME_LOG_HAS_DEBUG
    log_config(TAG, "Name", get_name().c_str());
    uartex_dump_config(TAG);
#endif
}

}  // namespace uartex
}  // namespace esphome
