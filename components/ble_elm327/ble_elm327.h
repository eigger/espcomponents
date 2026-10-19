#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif
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
  void set_formula(std::function<float(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, const std::vector<uint8_t> &)> f) { formula_ = f; }
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
  optional<std::function<float(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, const std::vector<uint8_t> &)>> formula_;
};

// ── Component ─────────────────────────────────────────────────────────────────
// Plain Component (no global polling). All timing is driven by loop().

class BleElm327Component : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
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
  // Appends user-defined init commands after the mandatory base sequence (see ble_elm327.cpp).
  // Duplicate commands (case-insensitive, trimmed) are ignored.
  void add_init_command(const std::string &cmd);
  void set_tx_delay(uint32_t ms) { tx_delay_ms_ = ms; }
  void set_response_timeout(uint32_t ms) { response_timeout_ms_ = ms; }

#ifdef USE_SENSOR
  void set_timeout_count_sensor(sensor::Sensor *s) { timeout_count_sensor_ = s; }
  void set_error_count_sensor(sensor::Sensor *s) { error_count_sensor_ = s; }
  void set_no_data_count_sensor(sensor::Sensor *s) { no_data_count_sensor_ = s; }
  void set_write_failure_count_sensor(sensor::Sensor *s) { write_failure_count_sensor_ = s; }
  void set_reinit_count_sensor(sensor::Sensor *s) { reinit_count_sensor_ = s; }
  void set_reconnect_count_sensor(sensor::Sensor *s) { reconnect_count_sensor_ = s; }
#endif
#ifdef USE_TEXT_SENSOR
  void set_last_error_text_sensor(text_sensor::TextSensor *s) { last_error_text_sensor_ = s; }
#endif

  // Enqueues a command (AT command, PID, etc.) to be transmitted to the ELM327 adapter.
  void send_command(const std::string &cmd);

 protected:
  enum class ElmState { IDLE, CONNECTED, READY };

  struct TxItem {
    std::string cmd;
    BleElm327Device *dev{nullptr};
    bool pre{false};  // a device's pre_command (e.g. ATSH)
  };

  // What the command currently in flight was queued as.
  enum class TxKind { INIT, PRE, DEVICE, USER };

  bool send_command_raw_(const std::string &cmd);
  void on_notify(const uint8_t *data, uint16_t length);
  void process_response(const std::string &response, bool expected);

  // Recovery
  void queue_init_commands_();
  void on_exchange_failure_();
  void fail_exchange_(uint32_t now);
  void handle_error_response_(const std::string &error, bool expected);
  void record_write_failure_();
  void reinitialize_();
  void reconnect_();
  void clear_tx_queue_();

  // Diagnostics
  void set_last_error_(const std::string &error);
  void flush_diagnostics_(uint32_t now);
#ifdef USE_SENSOR
  static void publish_count_(sensor::Sensor *s, uint32_t value);
#endif

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
  std::vector<std::string> extra_init_commands_;
  // Tracks pre_commands currently applied on the ELM327 (e.g., last ATSH).
  // When a device's pre_commands differs, they are re-queued before its PID request.
  std::vector<std::string> current_pre_commands_;

  // Device registry & unified TX queue (init commands: dev=nullptr, sensor: dev=device)
  std::vector<BleElm327Device *> devices_;
  std::queue<TxItem> tx_queue_;
  size_t collect_idx_{0};  // round-robin start index to prevent starvation

  // Timing
  uint32_t tx_delay_ms_{50};
  uint32_t response_timeout_ms_{1500};
  // Timeout applied to the command currently in flight (see ble_elm327.cpp for
  // when the longer init timeout is used).
  uint32_t current_timeout_ms_{1500};
  // Last time a command was written or a notify chunk arrived (idle timeout base).
  uint32_t last_activity_time_{0};
  // Time the command in flight was written (absolute timeout base).
  uint32_t last_write_time_{0};
  // Last time the exchange finished: '>' received, timeout, or write failure.
  // tx_delay is measured from here so a slow adapter gets idle time.
  uint32_t last_rx_time_{0};

  std::string rx_buffer_;
  std::string last_sent_command_;
  bool waiting_for_response_{false};
  TxKind in_flight_kind_{TxKind::USER};
  // The last init command is in flight: READY is entered only once it is
  // answered; on timeout / failed write it is re-sent.
  bool final_init_in_flight_{false};
  std::string final_init_command_;
  // The previous exchange timed out / the command in flight is the first one
  // written after a timeout (a STOPPED reply to it is not counted).
  bool last_exchange_timed_out_{false};
  bool follows_timeout_{false};
  // Use the long init timeout until a PID request gets a normal (non-error)
  // reply. Set on (re)init and after timeouts / error replies, because with
  // ATSP0 the next request may start a protocol search that takes seconds.
  bool long_timeout_until_data_{false};

  // Recovery state
  // Timeouts, failed writes and adapter-level error replies in a row. Reset only
  // by a normal reply to a PID request or init command.
  uint8_t consecutive_failures_{0};
  // Set once a re-init was done; the next failure streak escalates to a
  // reconnect. Cleared after several PID replies in a row succeed again.
  bool reinit_attempted_{false};
  uint8_t successes_since_reinit_{0};
  // reconnect_() requested a disconnect; loop() is paused until it happens.
  bool reconnect_pending_{false};
  uint32_t reconnect_requested_at_{0};
  // Recovery reconnects in a row (exponential backoff), cleared when PID replies
  // have recovered. Survives the disconnect it causes.
  uint8_t recovery_reconnects_{0};
  uint32_t last_reconnect_at_{0};

  // Diagnostics publishing is rate limited (see flush_diagnostics_()).
  bool diag_dirty_{false};
  uint32_t last_diag_publish_{0};

  // Diagnostic counters
  uint32_t timeout_count_{0};
  uint32_t error_count_{0};
  uint32_t no_data_count_{0};
  uint32_t write_failure_count_{0};
  uint32_t reinit_count_{0};
  uint32_t reconnect_count_{0};
  std::string last_error_;

#ifdef USE_SENSOR
  sensor::Sensor *timeout_count_sensor_{nullptr};
  sensor::Sensor *error_count_sensor_{nullptr};
  sensor::Sensor *no_data_count_sensor_{nullptr};
  sensor::Sensor *write_failure_count_sensor_{nullptr};
  sensor::Sensor *reinit_count_sensor_{nullptr};
  sensor::Sensor *reconnect_count_sensor_{nullptr};
#endif
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *last_error_text_sensor_{nullptr};
#endif
};

}  // namespace ble_elm327
}  // namespace esphome

#include "automation.h"
#endif
