#pragma once

#include "esphome/core/automation.h"
#include "uartex.h"

namespace esphome {
namespace uartex {

class TxTimeoutTrigger : public Trigger<> {
public:
  explicit TxTimeoutTrigger(UARTExComponent *parent) {
    parent->add_on_error_callback([this](const ERROR error) {
      if (error == ERROR_TX_TIMEOUT)
        this->trigger();
    });
  }
};

class WriteTrigger : public Trigger<const uint8_t *, const uint16_t> {
public:
  explicit WriteTrigger(UARTExComponent *parent) {
    parent->add_on_write_callback(
        [this](const uint8_t *data, const uint16_t len) {
          this->trigger(data, len);
        });
  }
};

class ReadTrigger : public Trigger<const uint8_t *, const uint16_t> {
public:
  explicit ReadTrigger(UARTExComponent *parent) {
    parent->add_on_read_callback(
        [this](const uint8_t *data, const uint16_t len) {
          this->trigger(data, len);
        });
  }
};

class MatchTrigger : public Trigger<cmd_t> {
public:
  explicit MatchTrigger(UARTExComponent *parent) {
    parent->add_on_match_callback(
        [this](const std::vector<uint8_t> &data, const state_t *state) {
          if (verify_state(data, state))
            this->trigger(cmd_t(data));
        });
  }
};

class MatchTrigger_v2 : public Trigger<cmd_t> {
public:
  explicit MatchTrigger_v2(UARTExComponent *parent, const state_t &state)
      : state_(state) {
    parent->add_on_match_callback(
        [this](const std::vector<uint8_t> &data, const state_t *state_ptr) {
          if (verify_state(data, &state_))
            this->trigger(cmd_t(data));
        });
  }

protected:
  state_t state_;
};

template <typename... Ts>
class UARTExWriteAction : public Action<Ts...>,
                          public Parented<UARTExComponent> {
public:
  void set_data_template(std::function<cmd_t(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const cmd_t &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

#if ESPHOME_VERSION_CODE >= VERSION_CODE(2025, 11, 0)
  void play(const Ts &...x) override
#else
  void play(Ts... x) override
#endif
  {
    if (this->static_) {
      this->parent_->enqueue_tx_data({nullptr, &this->data_static_});
    } else {
      data_static_ = this->data_func_(x...);
      this->parent_->enqueue_tx_data({nullptr, &this->data_static_});
    }
  }

protected:
  bool static_{false};
  std::function<cmd_t(Ts...)> data_func_{};
  cmd_t data_static_{};
};

} // namespace uartex
} // namespace esphome
