#pragma once

#include <array>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::tab5_c6_uart {

// Dedicated recovery firmware only: UART1 TX49/RX50, C6 BOOT via P4 GPIO0,
// C6 RESET via the existing P4 GPIO15 -> 1k resistor -> C6 EN connection.
class Tab5C6Uart : public Component {
 public:
  void setup() override {
    uart_config_t config{};
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;
    if (uart_param_config(UART_NUM_1, &config) != ESP_OK ||
        uart_set_pin(UART_NUM_1, 49, 50, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK ||
        uart_driver_install(UART_NUM_1, 8192, 0, 0, nullptr, 0) != ESP_OK) {
      this->mark_failed();
      return;
    }
    // baud_rate: 0 keeps the ESPHome logger off this driver.
    if (usb_serial_jtag_is_driver_installed()) {
      this->mark_failed();
      return;
    }
    usb_serial_jtag_driver_config_t usb_config{};
    usb_config.rx_buffer_size = 8192;
    usb_config.tx_buffer_size = 8192;
    if (usb_serial_jtag_driver_install(&usb_config) != ESP_OK) {
      this->mark_failed();
      return;
    }
    this->fast_loop_.start();
  }

  void enter_download_mode() {
    if (this->is_failed()) return;
    // Set latches before enabling outputs. BOOT is open drain: release to
    // the C6's own 3.3V pull-up, never drive its power rail from a P4 pin.
    gpio_set_level(GPIO_NUM_0, 1);
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(GPIO_NUM_15, 1);
    gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(GPIO_NUM_0, 0);
    delay(10);
    gpio_set_level(GPIO_NUM_15, 0);
    delay(100);
    gpio_set_level(GPIO_NUM_15, 1);
    delay(100);
    gpio_set_level(GPIO_NUM_0, 1);
    uart_flush_input(UART_NUM_1);
    this->ready_ = true;
  }

  void loop() override {
    if (!this->ready_) return;
    // Preserve partial writes; neither direction may block the ESPHome loop.
    if (this->to_c6_pos_ == this->to_c6_size_) {
      int n = usb_serial_jtag_read_bytes(this->to_c6_.data(), this->to_c6_.size(), 0);
      this->to_c6_size_ = n > 0 ? n : 0;
      this->to_c6_pos_ = 0;
    }
    if (this->to_c6_pos_ < this->to_c6_size_) {
      int n = uart_tx_chars(UART_NUM_1,
                           reinterpret_cast<const char *>(this->to_c6_.data() + this->to_c6_pos_),
                           this->to_c6_size_ - this->to_c6_pos_);
      if (n > 0) this->to_c6_pos_ += n;
    }
    if (this->to_pc_pos_ == this->to_pc_size_) {
      int n = uart_read_bytes(UART_NUM_1, this->to_pc_.data(), this->to_pc_.size(), 0);
      this->to_pc_size_ = n > 0 ? n : 0;
      this->to_pc_pos_ = 0;
    }
    if (this->to_pc_pos_ < this->to_pc_size_) {
      int n = usb_serial_jtag_write_bytes(this->to_pc_.data() + this->to_pc_pos_,
                                        this->to_pc_size_ - this->to_pc_pos_, 0);
      if (n > 0) this->to_pc_pos_ += n;
    }
  }

 protected:
  HighFrequencyLoopRequester fast_loop_;
  std::array<uint8_t, 512> to_c6_{};
  std::array<uint8_t, 512> to_pc_{};
  size_t to_c6_pos_{0}, to_c6_size_{0};
  size_t to_pc_pos_{0}, to_pc_size_{0};
  bool ready_{false};
};
}  // namespace esphome::tab5_c6_uart
