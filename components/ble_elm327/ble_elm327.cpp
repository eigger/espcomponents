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
// Minimum timeout for init commands (ATZ) and the first PID request after init
// (which may trigger an OBD protocol search).
static const uint32_t INIT_RESPONSE_TIMEOUT_MS = 5000;
// Consecutive timeouts / write failures before re-init, and again before reconnect.
static const uint8_t MAX_CONSECUTIVE_FAILURES = 3;

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
  if (elm_state_ == ElmState::IDLE) return;
  const uint32_t now = millis();

  if (waiting_for_response_) {
    // Measured from the last write or the last received chunk, so slow
    // multi-frame replies (or "SEARCHING...") keep the exchange alive.
    if (now - last_activity_time_ <= current_timeout_ms_) return;

    timeout_count_++;
    ESP_LOGW(TAG, "Timeout (%ums) waiting for response to '%s' (%u in a row)", (unsigned) current_timeout_ms_,
             last_sent_command_.c_str(), (unsigned) (consecutive_failures_ + 1));
#ifdef USE_SENSOR
    publish_count_(timeout_count_sensor_, timeout_count_);
#endif
    set_last_error_("TIMEOUT (" + last_sent_command_ + ")");
    waiting_for_response_ = false;
    // Drop any partial reply so it cannot be glued to the next command's response.
    rx_buffer_.clear();
    last_rx_time_ = now;
    on_exchange_failure_();
    return;
  }

  // Drain queued command (init or sensor). tx_delay is idle time after the
  // previous exchange finished (prompt received / timeout), not after the write.
  if (!tx_queue_.empty()) {
    if (now - last_rx_time_ < tx_delay_ms_) return;
    auto item = tx_queue_.front();
    tx_queue_.pop();
    if (item.dev) item.dev->on_dequeue();

    // Init commands (ATZ in particular), a manual ATZ and the first PID request
    // after init (may trigger a protocol search) get the longer timeout.
    bool long_timeout = elm_state_ == ElmState::CONNECTED || normalize_command(item.cmd) == "atz" ||
                        (item.dev != nullptr && long_timeout_next_request_);
    current_timeout_ms_ = long_timeout ? std::max(response_timeout_ms_, INIT_RESPONSE_TIMEOUT_MS) : response_timeout_ms_;
    if (item.dev != nullptr) long_timeout_next_request_ = false;
    last_sent_was_device_ = item.dev != nullptr;

    send_command_raw_(item.cmd);  // failures are counted and escalated inside

    if (elm_state_ == ElmState::CONNECTED && tx_queue_.empty()) {
      elm_state_ = ElmState::READY;
      ESP_LOGI(TAG, "ELM327 initialized and ready");
    }
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
          for (const auto &c : pre) tx_queue_.push({c, nullptr});
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
  ESP_LOGCONFIG(TAG, "  Base init commands : ATZ, ATE0, ATL0, ATS0, ATH0, ATSP0");
  ESP_LOGCONFIG(TAG, "  Extra init commands: %u", (unsigned)extra_init_commands_.size());
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Timeout count", timeout_count_sensor_);
  LOG_SENSOR("  ", "Error response count", error_count_sensor_);
  LOG_SENSOR("  ", "NO DATA count", no_data_count_sensor_);
  LOG_SENSOR("  ", "Write failure count", write_failure_count_sensor_);
  LOG_SENSOR("  ", "Re-init count", reinit_count_sensor_);
  LOG_SENSOR("  ", "Reconnect count", reconnect_count_sensor_);
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
      reinit_attempted_ = false;
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
      rx_buffer_.clear();
      queue_init_commands_();
      break;

    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == rx_char_handle_)
        on_notify(param->notify.value, param->notify.value_len);
      break;

    case ESP_GATTC_WRITE_CHAR_EVT:
      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Write failed, status=%d", param->write.status);
        write_failure_count_++;
#ifdef USE_SENSOR
        publish_count_(write_failure_count_sensor_, write_failure_count_);
#endif
        set_last_error_("WRITE FAILED");
      }
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
  long_timeout_next_request_ = true;
  ESP_LOGI(TAG, "Queued %u init commands (%u base + %u extra)", (unsigned) tx_queue_.size(),
           (unsigned)(sizeof(BASE_INIT_COMMANDS) / sizeof(BASE_INIT_COMMANDS[0])),
           (unsigned) extra_init_commands_.size());
}

void BleElm327Component::clear_tx_queue_() {
  while (!tx_queue_.empty()) tx_queue_.pop();
  for (auto *d : devices_) d->on_dequeue();
}

// Called on every timeout / failed write. After MAX_CONSECUTIVE_FAILURES in a
// row the adapter is re-initialized (ATZ + init sequence); if the next streak
// happens before any PID request was answered, the BLE link is dropped so
// ble_client reconnects from scratch.
void BleElm327Component::on_exchange_failure_() {
  if (++consecutive_failures_ < MAX_CONSECUTIVE_FAILURES) return;
  consecutive_failures_ = 0;
  if (!reinit_attempted_) {
    reinit_attempted_ = true;
    reinitialize_();
  } else {
    reconnect_();
  }
}

void BleElm327Component::reinitialize_() {
  reinit_count_++;
  ESP_LOGW(TAG, "%u consecutive failures — re-initializing ELM327", (unsigned) MAX_CONSECUTIVE_FAILURES);
#ifdef USE_SENSOR
  publish_count_(reinit_count_sensor_, reinit_count_);
#endif
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
  ESP_LOGW(TAG, "Adapter still not responding after re-init — disconnecting to force a BLE reconnect");
#ifdef USE_SENSOR
  publish_count_(reconnect_count_sensor_, reconnect_count_);
#endif
  set_last_error_("RECONNECT");
  clear_tx_queue_();
  current_pre_commands_.clear();
  rx_buffer_.clear();
  waiting_for_response_ = false;
  // ESP_GATTC_DISCONNECT_EVT resets the remaining state; ble_client reconnects.
  this->parent_->disconnect();
}

void BleElm327Component::set_last_error_(const std::string &error) {
  last_error_ = error;
#ifdef USE_TEXT_SENSOR
  if (last_error_text_sensor_ != nullptr &&
      (!last_error_text_sensor_->has_state() || last_error_text_sensor_->state != error)) {
    last_error_text_sensor_->publish_state(error);
  }
#endif
}

#ifdef USE_SENSOR
void BleElm327Component::publish_count_(sensor::Sensor *s, uint32_t value) {
  if (s != nullptr) s->publish_state(static_cast<float>(value));
}
#endif

bool BleElm327Component::send_command_raw_(const std::string &cmd) {
  if (client_state_ != espbt::ClientState::ESTABLISHED || tx_char_handle_ == 0) return false;
  last_sent_command_ = normalize_command(cmd);

  esp_err_t err = ESP_FAIL;
  auto *chr = this->parent_->get_characteristic(service_uuid_, tx_char_uuid_);
  if (chr == nullptr) {
    ESP_LOGW(TAG, "TX characteristic missing");
  } else {
    err = chr->write_value(reinterpret_cast<uint8_t *>(const_cast<char *>(cmd.data())), cmd.size(),
                           ESP_GATT_WRITE_TYPE_NO_RSP);
  }

  if (err != ESP_OK) {
    // Do not wait for a reply that cannot come; back off by tx_delay instead.
    write_failure_count_++;
    ESP_LOGW(TAG, "Write of '%s' failed: %s", last_sent_command_.c_str(), esp_err_to_name(err));
#ifdef USE_SENSOR
    publish_count_(write_failure_count_sensor_, write_failure_count_);
#endif
    set_last_error_("WRITE FAILED (" + last_sent_command_ + ")");
    waiting_for_response_ = false;
    last_rx_time_ = millis();
    on_exchange_failure_();
    return false;
  }

  ESP_LOGD(TAG, ">> %s", cmd.c_str());
  // Anything still buffered arrived after the previous prompt and is stale.
  rx_buffer_.clear();
  waiting_for_response_ = true;
  last_activity_time_ = millis();
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
    consecutive_failures_ = 0;  // the adapter is alive
    if (!expected) {
      ESP_LOGW(TAG, "Response received with no command pending (late reply after a timeout?) — consider increasing response_timeout");
    }
    process_response(response, expected);
  }
}

void BleElm327Component::process_response(const std::string &response, bool expected) {
  ESP_LOGD(TAG, "<< %s", response.c_str());

  std::string error = classify_error_response(response, last_sent_command_);
  if (!error.empty()) {
    if (error == "NO DATA") {
      no_data_count_++;
      ESP_LOGD(TAG, "NO DATA for '%s'", last_sent_command_.c_str());
#ifdef USE_SENSOR
      publish_count_(no_data_count_sensor_, no_data_count_);
#endif
    } else {
      error_count_++;
      ESP_LOGW(TAG, "ELM327 replied '%s' to '%s'", error.c_str(), last_sent_command_.c_str());
#ifdef USE_SENSOR
      publish_count_(error_count_sensor_, error_count_);
#endif
    }
    set_last_error_(error + " (" + last_sent_command_ + ")");
    return;
  }

  // A PID request was answered: the current failure streak is over, so a
  // future one starts again with a re-init before escalating to reconnect.
  if (expected && last_sent_was_device_) reinit_attempted_ = false;

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
