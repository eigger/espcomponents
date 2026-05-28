#pragma once

#include "esphome/core/component.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include <functional>
#include <queue>
#include <string>
#include <vector>

#ifdef USE_ESP32
#include <esp_gattc_api.h>

namespace esphome {
namespace ble_elm327 {

namespace espbt = esphome::esp32_ble_tracker;

// ── Device base ──────────────────────────────────────────────────────────────
// Each sub-platform (sensor, binary_sensor, …) extends this.
// Inherits PollingComponent so every device owns its own update_interval.

class BleElm327Device : public PollingComponent {
 public:
  void update() override { enqueued_ = true; }
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Returns true and marks in_queue_ only if update() fired and device is not already queued.
  // Prevents the same device from accumulating in tx_queue_ when update_interval < loop drain rate.
  bool consume_enqueued() {
    if (!enqueued_ || in_queue_) return false;
    enqueued_ = false;
    in_queue_ = true;
    return true;
  }

  // Called when the device is popped from tx_queue_, allowing re-enqueue on next update().
  void on_dequeue() { in_queue_ = false; }

  void set_pid(const std::string &pid) { pid_ = pid; }
  void set_mode(const std::string &mode) { mode_ = mode; }
  void set_formula(std::function<float(uint8_t, uint8_t, uint8_t, uint8_t)> f) { formula_ = f; }
  void add_pre_command(const std::string &cmd) { pre_commands_.push_back(cmd + "\r"); }
  const std::vector<std::string> &get_pre_commands() const { return pre_commands_; }

  std::string get_command() const { return mode_ + pid_ + "\r"; }
  std::string get_pid() const { return pid_; }
  std::string get_mode() const { return mode_; }

  // Called by the component with every parsed response frame.
  // Checks mode+PID match; calls publish_data() if matched.
  bool on_receive(const std::vector<uint8_t> &bytes);

  virtual void publish_data(const std::vector<uint8_t> &data) {}
  virtual void dump_config() {}

 protected:
  float parse_float(const std::vector<uint8_t> &data);

  bool enqueued_{false};
  bool in_queue_{false};
  std::string pid_;
  std::string mode_{"01"};
  std::vector<std::string> pre_commands_;
  optional<std::function<float(uint8_t, uint8_t, uint8_t, uint8_t)>> formula_;
};

// ── Component ─────────────────────────────────────────────────────────────────
// Plain Component (no global polling). All timing is driven by loop().

class BleElm327Component : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override {}
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                            esp_ble_gattc_cb_param_t *param) override;

  void set_service_uuid16(uint16_t uuid) { service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void set_rx_char_uuid16(uint16_t uuid) { rx_char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_rx_char_uuid32(uint32_t uuid) { rx_char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_rx_char_uuid128(uint8_t *uuid) { rx_char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void set_tx_char_uuid16(uint16_t uuid) { tx_char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_tx_char_uuid32(uint32_t uuid) { tx_char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_tx_char_uuid128(uint8_t *uuid) { tx_char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void add_device(BleElm327Device *d) { devices_.push_back(d); }
  void add_init_command(const std::string &cmd) { init_commands_.push_back(cmd + "\r"); }
  void set_tx_delay(uint32_t ms) { tx_delay_ms_ = ms; }

 protected:
  enum class ElmState { IDLE, CONNECTED, READY };

  struct TxItem {
    std::string cmd;
    BleElm327Device *dev{nullptr};
  };

  bool send_command(const std::string &cmd);
  void on_notify(const uint8_t *data, uint16_t length);
  void process_response(const std::string &response);

  // BLE handles
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID rx_char_uuid_;
  espbt::ESPBTUUID tx_char_uuid_;
  uint16_t rx_char_handle_{0};
  uint16_t tx_char_handle_{0};
  esp_gatt_if_t gattc_if_{0};
  esp_bd_addr_t remote_bda_{};
  espbt::ClientState client_state_{espbt::ClientState::IDLE};

  // State machine
  ElmState elm_state_{ElmState::IDLE};
  std::vector<std::string> init_commands_;
  // Tracks pre_commands currently applied on the ELM327 (e.g., last ATSH).
  // When a device's pre_commands differs, they are re-queued before its PID request.
  std::vector<std::string> current_pre_commands_;

  // Device registry & unified TX queue (init commands: dev=nullptr, sensor: dev=device)
  std::vector<BleElm327Device *> devices_;
  std::queue<TxItem> tx_queue_;
  size_t collect_idx_{0};  // round-robin start index to prevent starvation

  // Timing
  uint32_t tx_delay_ms_{50};
  uint32_t last_tx_time_{0};

};

}  // namespace ble_elm327
}  // namespace esphome
#endif
