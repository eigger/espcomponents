#include "ble_elm327.h"
#include "response_parser.h"
#include "esphome/core/log.h"

#include <algorithm>

#ifdef USE_ESP32

namespace esphome {
namespace ble_elm327 {

static const char *const TAG = "ble_elm327";
// Always sent first on connect, in this order, before any YAML init_commands.
static const char *const BASE_INIT_COMMANDS[] = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0"};
// Minimum timeout for init commands (ATZ), and for every command until a PID
// request gets a normal reply after (re)init, a timeout or an error reply
// (an ATSP0 protocol search can take several seconds).
static const uint32_t INIT_RESPONSE_TIMEOUT_MS = 5000;
// Normal PID replies in a row needed after a re-init before the next failure
// streak starts with a re-init again instead of a reconnect.
static const uint8_t SUCCESSES_TO_CLEAR_RECOVERY = 5;
// Bytes buffered without a '>' prompt before the buffer is discarded.
static const size_t MAX_RX_BUFFER_SIZE = 4096;
// Minimum interval between diagnostic entity publishes.
static const uint32_t DIAG_PUBLISH_INTERVAL_MS = 5000;
// Re-issue the disconnect if no disconnect event arrived within this time.
static const uint32_t RECONNECT_RETRY_MS = 10000;

// ── BleElm327Device ─────────────────────────────────────────────────────────

bool BleElm327Device::on_receive(const std::vector<uint8_t> &bytes) {
  size_t skip = (pid_.size() == 2) ? 2 : 3;
  if (bytes.size() <= skip) return false;

  // Response code = 0x40 + mode (hex). Mode "01" → 0x41, mode "22" → 0x62.
  uint8_t expected_code = 0x40 + static_cast<uint8_t>(std::stoul(mode_, nullptr, 16));
  if (bytes[0] != expected_code) return false;

  if (pid_.size() == 2) {
    if (bytes[1] != static_cast<uint8_t>(std::stoul(pid_, nullptr, 16))) return false;
  } else {
    uint8_t pid_hi = static_cast<uint8_t>(std::stoul(pid_.substr(0, 2), nullptr, 16));
    uint8_t pid_lo = static_cast<uint8_t>(std::stoul(pid_.substr(2, 2), nullptr, 16));
    if (bytes[1] != pid_hi || bytes[2] != pid_lo) return false;
  }

  std::vector<uint8_t> data(bytes.begin() + skip, bytes.end());
  publish_data(data);
  return true;
}

float BleElm327Device::parse_float(const std::vector<uint8_t> &data) {
  if (formula_.has_value()) {
    uint8_t a = data.size() > 0 ? data[0] : 0;
    uint8_t b = data.size() > 1 ? data[1] : 0;
    uint8_t c = data.size() > 2 ? data[2] : 0;
    uint8_t d = data.size() > 3 ? data[3] : 0;
    uint8_t e = data.size() > 4 ? data[4] : 0;
    uint8_t f = data.size() > 5 ? data[5] : 0;
    return (*formula_)(a, b, c, d, e, f, data);
  }
  float val = 0;
  for (size_t i = 0; i < data.size(); i++) val = val * 256.0f + data[i];
  return val;
}

void BleElm327Component::add_init_command(const std::string &cmd) {
  const std::string normalized = normalize_command(cmd);
  if (normalized.empty()) return;

  for (const char *base_cmd : BASE_INIT_COMMANDS) {
    if (normalized == normalize_command(base_cmd)) {
      ESP_LOGD(TAG, "Skip duplicate init command (already in base): %s", normalized.c_str());
      return;
    }
  }

  for (const auto &extra_cmd : extra_init_commands_) {
    if (normalized == normalize_command(extra_cmd)) {
      ESP_LOGD(TAG, "Skip duplicate init command (already queued): %s", normalized.c_str());
      return;
    }
  }

  extra_init_commands_.push_back(normalized + "\r");
}

void BleElm327Component::send_command(const std::string &cmd) {
  if (cmd.empty()) return;
  std::string formatted = cmd;
  if (formatted.back() != '\r') {
    formatted += '\r';
  }
  tx_queue_.push({formatted, nullptr});
}

// ── BleElm327Component ──────────────────────────────────────────────────────

void BleElm327Component::setup() {
#ifdef USE_SENSOR
  // Publish initial zeros so the diagnostic counters are not "unknown".
  publish_count_(timeout_count_sensor_, 0);
  publish_count_(error_count_sensor_, 0);
  publish_count_(no_data_count_sensor_, 0);
  publish_count_(write_failure_count_sensor_, 0);
  publish_count_(reinit_count_sensor_, 0);
  publish_count_(reconnect_count_sensor_, 0);
#endif
}

void BleElm327Component::loop() {
  const uint32_t now = millis();
  flush_diagnostics_(now);

  if (reconnect_pending_) {
    // Paused until ESP_GATTC_DISCONNECT_EVT. Retry if the disconnect got lost.
    if (now - reconnect_requested_at_ > RECONNECT_RETRY_MS) {
      ESP_LOGW(TAG, "Still connected %ums after requesting a disconnect — retrying", (unsigned) RECONNECT_RETRY_MS);
      reconnect_requested_at_ = now;
      this->parent_->disconnect();
    }
    return;
  }

  if (elm_state_ == ElmState::IDLE) return;

  if (waiting_for_response_) {
    const TimeoutCheck check =
        check_response_timeout(auto_recovery_, current_timeout_ms_, is_protocol_search_in_progress(rx_buffer_),
                               now - last_write_time_, now - last_activity_time_);
    if (!check.expired) return;

    timeout_count_++;
    if (check.hard_limit_hit) {
      ESP_LOGW(TAG, "Timeout: no '>' prompt %ums after sending '%s' (%u failures in a row)",
               (unsigned) check.hard_limit_ms, last_sent_command_.c_str(), (unsigned) (consecutive_failures_ + 1));
    } else {
      ESP_LOGW(TAG, "Timeout (%ums) waiting for response to '%s' (%u failures in a row)", (unsigned) check.timeout_ms,
               last_sent_command_.c_str(), (unsigned) (consecutive_failures_ + 1));
    }
    set_last_error_("TIMEOUT (" + last_sent_command_ + ")");
    // The adapter may have been searching for a protocol without saying so.
    long_timeout_until_data_ = true;
    last_exchange_timed_out_ = true;
    fail_exchange_(now);
    return;
  }

  // Drain queued command (init or sensor). tx_delay is idle time after the
  // previous exchange finished (prompt received / timeout), not after the write.
  if (!tx_queue_.empty()) {
    if (now - last_rx_time_ < tx_delay_ms_) return;
    auto item = tx_queue_.front();
    tx_queue_.pop();
    if (item.dev) item.dev->on_dequeue();

    if (elm_state_ == ElmState::CONNECTED) {
      in_flight_kind_ = TxKind::INIT;
      // READY is entered when this last init command is answered (on_notify).
      final_init_in_flight_ = tx_queue_.empty();
      if (final_init_in_flight_) final_init_command_ = item.cmd;
    } else if (item.pre) {
      in_flight_kind_ = TxKind::PRE;
    } else if (item.dev != nullptr) {
      in_flight_kind_ = TxKind::DEVICE;
    } else {
      in_flight_kind_ = TxKind::USER;
    }

    // Init commands, a manual ATZ, and every command until a PID request gets a
    // normal reply after (re)init / a timeout / an error reply use the long
    // timeout, so an ATSP0 protocol search is never cut short.
    bool long_timeout = in_flight_kind_ == TxKind::INIT || long_timeout_until_data_ ||
                        normalize_command(item.cmd) == "atz";
    current_timeout_ms_ = long_timeout ? std::max(response_timeout_ms_, INIT_RESPONSE_TIMEOUT_MS) : response_timeout_ms_;

    follows_timeout_ = last_exchange_timed_out_;
    last_exchange_timed_out_ = false;

    send_command_raw_(item.cmd);  // failures are counted and escalated inside
    return;
  }

  if (elm_state_ != ElmState::READY) return;

  // Collect one ready device per loop (round-robin to prevent starvation)
  if (!devices_.empty()) {
    size_t n = devices_.size();
    for (size_t i = 0; i < n; i++) {
      auto *d = devices_[(collect_idx_ + i) % n];
      if (d->consume_enqueued()) {
        const auto &pre = d->get_pre_commands();
        if (pre != current_pre_commands_) {
          for (const auto &c : pre) tx_queue_.push({c, nullptr, true});
          current_pre_commands_ = pre;
        }
        tx_queue_.push({d->get_command(), d});
        collect_idx_ = (collect_idx_ + i + 1) % n;
        break;
      }
    }
  }
}

void BleElm327Component::dump_config() {
  char uuid_buf[espbt::UUID_STR_LEN];
  ESP_LOGCONFIG(TAG, "BLE ELM327:");
  ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str());
  ESP_LOGCONFIG(TAG, "  Service UUID       : %s", service_uuid_.to_str(uuid_buf));
  ESP_LOGCONFIG(TAG, "  RX Char UUID       : %s", rx_char_uuid_.to_str(uuid_buf));
  ESP_LOGCONFIG(TAG, "  TX Char UUID       : %s", tx_char_uuid_.to_str(uuid_buf));
  ESP_LOGCONFIG(TAG, "  TX delay           : %lums", (unsigned long) tx_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Response timeout   : %lums (init: %lums)", (unsigned long) response_timeout_ms_,
                (unsigned long) std::max(response_timeout_ms_, INIT_RESPONSE_TIMEOUT_MS));
  if (auto_recovery_) {
    ESP_LOGCONFIG(TAG, "  Auto recovery      : on (threshold %u)", (unsigned) recovery_threshold_);
  } else {
    ESP_LOGCONFIG(TAG, "  Auto recovery      : off");
  }
  ESP_LOGCONFIG(TAG, "  Base init commands : ATZ, ATE0, ATL0, ATS0, ATH0, ATSP0");
  ESP_LOGCONFIG(TAG, "  Extra init commands: %u", (unsigned)extra_init_commands_.size());
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Timeout count", timeout_count_sensor_);
  LOG_SENSOR("  ", "Error response count", error_count_sensor_);
  LOG_SENSOR("  ", "NO DATA count", no_data_count_sensor_);
  LOG_SENSOR("  ", "Write failure count", write_failure_count_sensor_);
  LOG_SENSOR("  ", "Re-init count", reinit_count_sensor_);
  LOG_SENSOR("  ", "Reconnect count", reconnect_count_sensor_);
  LOG_SENSOR("  ", "Response time", response_time_sensor_);
#endif
#ifdef USE_TEXT_SENSOR
  LOG_TEXT_SENSOR("  ", "Last error", last_error_text_sensor_);
#endif
  ESP_LOGCONFIG(TAG, "  Devices            : %u", (unsigned)devices_.size());
  for (auto *d : devices_) d->dump_config();
}

void BleElm327Component::gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                              esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_OPEN_EVT:
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Connection failed, status=%d", param->open.status);
        break;
      }
      gattc_if_ = gattc_if;
      memcpy(remote_bda_, param->open.remote_bda, sizeof(esp_bd_addr_t));
      client_state_ = espbt::ClientState::ESTABLISHED;
      ESP_LOGI(TAG, "Connected to ELM327");
      break;

    case ESP_GATTC_DISCONNECT_EVT:
      elm_state_ = ElmState::IDLE;
      client_state_ = espbt::ClientState::IDLE;
      rx_char_handle_ = 0;
      tx_char_handle_ = 0;
      clear_tx_queue_();
      current_pre_commands_.clear();
      rx_buffer_.clear();
      last_sent_command_.clear();
      waiting_for_response_ = false;
      consecutive_failures_ = 0;
      // After our own recovery disconnect keep reinit_attempted_, so the next
      // failure streak goes to the (backed-off) reconnect instead of starting
      // another re-init -> reconnect cycle.
      if (!reconnect_pending_) reinit_attempted_ = false;
      successes_since_reinit_ = 0;
      reconnect_pending_ = false;
      final_init_in_flight_ = false;
      last_exchange_timed_out_ = false;
      set_last_error_("DISCONNECTED");
      ESP_LOGW(TAG, "Disconnected from ELM327");
      break;

    case ESP_GATTC_SEARCH_CMPL_EVT: {
      auto *rx_chr = this->parent_->get_characteristic(service_uuid_, rx_char_uuid_);
      if (rx_chr == nullptr) { ESP_LOGW(TAG, "RX characteristic not found"); break; }
      rx_char_handle_ = rx_chr->handle;

      auto *tx_chr = this->parent_->get_characteristic(service_uuid_, tx_char_uuid_);
      if (tx_chr == nullptr) { ESP_LOGW(TAG, "TX characteristic not found"); break; }
      tx_char_handle_ = tx_chr->handle;

      ESP_LOGI(TAG, "RX handle=%d TX handle=%d — registering notify", rx_char_handle_, tx_char_handle_);
      auto status = esp_ble_gattc_register_for_notify(gattc_if_, remote_bda_, rx_char_handle_);
      if (status != ESP_GATT_OK) { ESP_LOGW(TAG, "Register notify failed: %d", status); }
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Notify registration failed: %d", param->reg_for_notify.status);
        break;
      }
      last_rx_time_ = millis();
      last_activity_time_ = last_rx_time_;
      elm_state_ = ElmState::CONNECTED;
      waiting_for_response_ = false;
      consecutive_failures_ = 0;
      reconnect_pending_ = false;
      final_init_in_flight_ = false;
      last_exchange_timed_out_ = false;
      rx_buffer_.clear();
      queue_init_commands_();
      break;

    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == rx_char_handle_)
        on_notify(param->notify.value, param->notify.value_len);
      break;

    case ESP_GATTC_WRITE_CHAR_EVT:
      // Only our TX characteristic; other nodes on the same ble_client write too.
      if (param->write.handle != tx_char_handle_ || param->write.status == ESP_GATT_OK) break;
      if (param->write.status == ESP_GATT_CONGESTED) {
        // For write-without-response, Bluedroid reports CONGESTED when the
        // packet was accepted but the L2CAP channel is congested
        // (att_protocol.c: "ATT congested, message accepted"). Not a failure.
        ESP_LOGD(TAG, "Write of '%s' accepted, link congested", last_sent_command_.c_str());
        break;
      }
      ESP_LOGW(TAG, "Write of '%s' failed, status=0x%02x", last_sent_command_.c_str(), param->write.status);
      record_write_failure_();
      // The adapter never got the command: end the exchange now instead of
      // also counting a timeout for it later.
      if (waiting_for_response_) fail_exchange_(millis());
      break;

    default:
      break;
  }
}

void BleElm327Component::queue_init_commands_() {
  for (const char *cmd : BASE_INIT_COMMANDS) {
    std::string framed = std::string(cmd) + "\r";
    tx_queue_.push({framed, nullptr});
  }
  for (const auto &cmd : extra_init_commands_) tx_queue_.push({cmd, nullptr});
  long_timeout_until_data_ = true;
  ESP_LOGI(TAG, "Queued %u init commands (%u base + %u extra)", (unsigned) tx_queue_.size(),
           (unsigned)(sizeof(BASE_INIT_COMMANDS) / sizeof(BASE_INIT_COMMANDS[0])),
           (unsigned) extra_init_commands_.size());
}

void BleElm327Component::clear_tx_queue_() {
  while (!tx_queue_.empty()) tx_queue_.pop();
  for (auto *d : devices_) d->on_dequeue();
}

// Ends the exchange in flight after a timeout or failed write and counts it.
void BleElm327Component::fail_exchange_(uint32_t now) {
  waiting_for_response_ = false;
  // Drop any partial reply so it cannot be glued to the next command's response.
  rx_buffer_.clear();
  last_rx_time_ = now;  // tx_delay counts from here
  // A header command that did not go through: re-send it before the next PID.
  if (in_flight_kind_ == TxKind::PRE) current_pre_commands_.clear();
  const bool retry_final_init = final_init_in_flight_;
  final_init_in_flight_ = false;

  on_exchange_failure_();

  if (!retry_final_init || elm_state_ != ElmState::CONNECTED || !tx_queue_.empty()) return;
  if (auto_recovery_) {
    // The last init command got no answer: re-send it rather than declaring
    // READY (repeated failures escalate to a re-init / reconnect).
    tx_queue_.push({final_init_command_, nullptr});
  } else {
    // Default: carry on as before auto_recovery existed.
    elm_state_ = ElmState::READY;
    ESP_LOGI(TAG, "ELM327 initialized and ready (last init command got no reply)");
  }
}

// Called for every timeout, failed write and adapter-level error reply. With
// auto_recovery, after recovery_threshold failures in a row the adapter is
// re-initialized (ATZ + init sequence); if the next streak comes before PID
// replies have recovered, the BLE link is dropped so ble_client reconnects.
// Without auto_recovery nothing else happens (the failures are only counted).
void BleElm327Component::on_exchange_failure_() {
  successes_since_reinit_ = 0;
  if (consecutive_failures_ < 255) consecutive_failures_++;
  if (!auto_recovery_) return;
  if (consecutive_failures_ < recovery_threshold_) return;
  consecutive_failures_ = 0;
  if (!reinit_attempted_) {
    reinit_attempted_ = true;
    reinitialize_();
    return;
  }
  const uint32_t backoff = reconnect_backoff_ms(recovery_reconnects_);
  const uint32_t since_last = millis() - last_reconnect_at_;
  if (recovery_reconnects_ > 0 && since_last < backoff) {
    ESP_LOGW(TAG, "Reconnect held off for %us more (backoff after %u recovery reconnects)",
             (unsigned) ((backoff - since_last) / 1000), (unsigned) recovery_reconnects_);
    return;
  }
  reconnect_();
}

void BleElm327Component::handle_error_response_(const std::string &error, bool expected) {
  if (error == "NO DATA") {
    // Normal for unsupported PIDs / ignition off: counted, not a "last error".
    no_data_count_++;
    diag_dirty_ = true;
    ESP_LOGD(TAG, "NO DATA for '%s'", last_sent_command_.c_str());
  } else {
    error_count_++;
    if (is_bus_error(error)) {
      // Vehicle side (e.g. ignition off): expected, keep the log quiet.
      ESP_LOGD(TAG, "ELM327 replied '%s' to '%s'", error.c_str(), last_sent_command_.c_str());
    } else {
      ESP_LOGW(TAG, "ELM327 replied '%s' to '%s'", error.c_str(), last_sent_command_.c_str());
    }
    set_last_error_(error + " (" + last_sent_command_ + ")");
  }
  // A late reply belongs to an exchange that already timed out (and was counted).
  if (!expected) return;

  // ATSP0 may (re)start a protocol search on the next request.
  if (error != "?") long_timeout_until_data_ = true;
  if (in_flight_kind_ == TxKind::PRE) current_pre_commands_.clear();
  if (error == "STOPPED" && follows_timeout_) {
    ESP_LOGD(TAG, "STOPPED right after a timeout: the adapter aborted the previous request");
  }
  if (counts_as_link_failure(error, last_sent_command_, follows_timeout_)) on_exchange_failure_();
}

void BleElm327Component::record_write_failure_() {
  write_failure_count_++;
  set_last_error_("WRITE FAILED (" + last_sent_command_ + ")");
}

void BleElm327Component::reinitialize_() {
  reinit_count_++;
  successes_since_reinit_ = 0;
  final_init_in_flight_ = false;
  ESP_LOGW(TAG, "%u consecutive failures — re-initializing ELM327", (unsigned) recovery_threshold_);
  set_last_error_("REINIT");
  clear_tx_queue_();
  // ATZ resets the adapter, including any ATSH header applied by pre_commands.
  current_pre_commands_.clear();
  rx_buffer_.clear();
  waiting_for_response_ = false;
  elm_state_ = ElmState::CONNECTED;
  queue_init_commands_();
}

void BleElm327Component::reconnect_() {
  reconnect_count_++;
  final_init_in_flight_ = false;
  if (recovery_reconnects_ < 255) recovery_reconnects_++;
  last_reconnect_at_ = millis();
  ESP_LOGW(TAG, "Adapter still failing after re-init — disconnecting to force a BLE reconnect");
  set_last_error_("RECONNECT");
  clear_tx_queue_();
  current_pre_commands_.clear();
  rx_buffer_.clear();
  waiting_for_response_ = false;
  // Stop all traffic until ESP_GATTC_DISCONNECT_EVT resets the state; the
  // ble_client (auto_connect) then reconnects and runs the init sequence again.
  elm_state_ = ElmState::IDLE;
  reconnect_pending_ = true;
  reconnect_requested_at_ = millis();
  this->parent_->disconnect();
}

void BleElm327Component::set_last_error_(const std::string &error) {
  last_error_ = error;
  diag_dirty_ = true;
}

// Diagnostic entities are published at most every DIAG_PUBLISH_INTERVAL_MS and
// only when a value changed, so a noisy link cannot flood the API / MQTT.
void BleElm327Component::flush_diagnostics_(uint32_t now) {
  if (!diag_dirty_ || now - last_diag_publish_ < DIAG_PUBLISH_INTERVAL_MS) return;
  diag_dirty_ = false;
  last_diag_publish_ = now;
#ifdef USE_SENSOR
  publish_count_(timeout_count_sensor_, timeout_count_);
  publish_count_(error_count_sensor_, error_count_);
  publish_count_(no_data_count_sensor_, no_data_count_);
  publish_count_(write_failure_count_sensor_, write_failure_count_);
  publish_count_(reinit_count_sensor_, reinit_count_);
  publish_count_(reconnect_count_sensor_, reconnect_count_);
  // Only the latest sample is published per interval.
  if (has_response_time_) publish_count_(response_time_sensor_, last_response_time_ms_);
#endif
#ifdef USE_TEXT_SENSOR
  if (last_error_text_sensor_ != nullptr && !last_error_.empty() &&
      (!last_error_text_sensor_->has_state() || last_error_text_sensor_->state != last_error_)) {
    last_error_text_sensor_->publish_state(last_error_);
  }
#endif
}

#ifdef USE_SENSOR
void BleElm327Component::publish_count_(sensor::Sensor *s, uint32_t value) {
  if (s == nullptr) return;
  const float v = static_cast<float>(value);
  if (s->has_state() && s->state == v) return;
  s->publish_state(v);
}
#endif

bool BleElm327Component::send_command_raw_(const std::string &cmd) {
  last_sent_command_ = normalize_command(cmd);

  esp_err_t err = ESP_FAIL;
  auto *chr = (client_state_ == espbt::ClientState::ESTABLISHED && tx_char_handle_ != 0)
                  ? this->parent_->get_characteristic(service_uuid_, tx_char_uuid_)
                  : nullptr;
  if (chr == nullptr) {
    // Not connected (or TX characteristic gone): handled like a failed write so
    // the command is not silently dropped and a stuck link escalates.
    ESP_LOGW(TAG, "Cannot send '%s': not connected / TX characteristic missing", last_sent_command_.c_str());
  } else {
    err = chr->write_value(reinterpret_cast<uint8_t *>(const_cast<char *>(cmd.data())), cmd.size(),
                           ESP_GATT_WRITE_TYPE_NO_RSP);
  }

  const uint32_t now = millis();
  if (err != ESP_OK) {
    // Do not wait for a reply that cannot come; back off by tx_delay instead.
    ESP_LOGW(TAG, "Write of '%s' failed: %s", last_sent_command_.c_str(), esp_err_to_name(err));
    record_write_failure_();
    fail_exchange_(now);
    return false;
  }

  ESP_LOGD(TAG, ">> %s", cmd.c_str());
  // Anything still buffered arrived after the previous prompt and is stale.
  rx_buffer_.clear();
  waiting_for_response_ = true;
  last_activity_time_ = now;
  last_write_time_ = now;
  return true;
}

void BleElm327Component::on_notify(const uint8_t *data, uint16_t length) {
  rx_buffer_.append(reinterpret_cast<const char *>(data), length);
  last_activity_time_ = millis();
  size_t pos;
  while ((pos = rx_buffer_.find('>')) != std::string::npos) {
    std::string response = rx_buffer_.substr(0, pos);
    rx_buffer_.erase(0, pos + 1);
    bool expected = waiting_for_response_;
    waiting_for_response_ = false;
    last_rx_time_ = millis();
    if (!expected) {
      ESP_LOGW(TAG, "Response received with no command pending (late reply after a timeout?) — "
                    "consider increasing response_timeout");
    }
    if (expected && final_init_in_flight_) {
      // Any reply (even "?" for an unsupported AT command) means the adapter
      // processed the whole init sequence.
      final_init_in_flight_ = false;
      if (elm_state_ == ElmState::CONNECTED) {
        elm_state_ = ElmState::READY;
        ESP_LOGI(TAG, "ELM327 initialized and ready");
      }
    }
    process_response(response, expected);
  }

  // A prompt never arrived and the adapter keeps talking: drop the data.
  // The exchange itself ends via the absolute timeout in loop().
  if (rx_buffer_.size() > MAX_RX_BUFFER_SIZE) {
    ESP_LOGW(TAG, "RX buffer exceeded %u bytes without a '>' prompt — discarding", (unsigned) MAX_RX_BUFFER_SIZE);
    rx_buffer_.clear();
    error_count_++;
    set_last_error_("RX OVERFLOW (" + last_sent_command_ + ")");
  }
}

void BleElm327Component::process_response(const std::string &response, bool expected) {
  ESP_LOGD(TAG, "<< %s", response.c_str());

  std::string error = classify_error_response(response, last_sent_command_);
  if (!error.empty()) {
    handle_error_response_(error, expected);
    return;
  }

  // A normal reply to a PID request or init command ends the failure streak.
  // Late replies (expected == false) never do: they follow a counted timeout.
  if (expected && (in_flight_kind_ == TxKind::DEVICE || in_flight_kind_ == TxKind::INIT)) {
    consecutive_failures_ = 0;
  }
  if (expected && in_flight_kind_ == TxKind::DEVICE) {
    // Response time: successful write → '>' of a normal PID reply. AT / init
    // commands, error replies, timeouts and late replies are not sampled.
    last_response_time_ms_ = millis() - last_write_time_;
    has_response_time_ = true;
#ifdef USE_SENSOR
    if (response_time_sensor_ != nullptr) diag_dirty_ = true;
#endif
    long_timeout_until_data_ = false;
    if ((reinit_attempted_ || recovery_reconnects_ > 0) &&
        ++successes_since_reinit_ >= SUCCESSES_TO_CLEAR_RECOVERY) {
      // Healthy again: the next failure streak starts with a re-init, not a reconnect.
      reinit_attempted_ = false;
      recovery_reconnects_ = 0;
      successes_since_reinit_ = 0;
    }
  }

  if (elm_state_ != ElmState::READY) return;

  std::vector<uint8_t> bytes = parse_response_bytes(response, last_sent_command_);
  if (bytes.empty()) return;

  // Broadcast to all devices — each checks its own mode+PID and updates if matched
  for (auto *d : devices_) {
    d->on_receive(bytes);
  }
}

}  // namespace ble_elm327
}  // namespace esphome
#endif
