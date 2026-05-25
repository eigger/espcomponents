#include "ble_elm327.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace ble_elm327 {

static const char *const TAG = "ble_elm327";

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
    return (*formula_)(a, b, c, d);
  }
  float val = 0;
  for (size_t i = 0; i < data.size(); i++) val = val * 256.0f + data[i];
  return val;
}

// ── BleElm327Component ──────────────────────────────────────────────────────

void BleElm327Component::loop() {
  if (elm_state_ == ElmState::IDLE) return;

  // Drain queued command (init or sensor) with tx_delay
  if (!tx_queue_.empty()) {
    if (millis() - last_tx_time_ < tx_delay_ms_) return;
    auto item = tx_queue_.front();
    tx_queue_.pop();
    if (item.dev) item.dev->on_dequeue();
    send_command(item.cmd);
    last_tx_time_ = millis();
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
        tx_queue_.push({d->get_command(), d});
        collect_idx_ = (collect_idx_ + i + 1) % n;
        break;
      }
    }
  }
}

void BleElm327Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE ELM327:");
  ESP_LOGCONFIG(TAG, "  MAC address        : %s", this->parent_->address_str());
  char service_uuid_str[esphome::esp32_ble::UUID_STR_LEN] = {0};
  char rx_char_uuid_str[esphome::esp32_ble::UUID_STR_LEN] = {0};
  char tx_char_uuid_str[esphome::esp32_ble::UUID_STR_LEN] = {0};
  ESP_LOGCONFIG(TAG, "  Service UUID       : %s", service_uuid_.to_str(service_uuid_str));
  ESP_LOGCONFIG(TAG, "  RX Char UUID       : %s", rx_char_uuid_.to_str(rx_char_uuid_str));
  ESP_LOGCONFIG(TAG, "  TX Char UUID       : %s", tx_char_uuid_.to_str(tx_char_uuid_str));
  ESP_LOGCONFIG(TAG, "  TX delay           : %ums", tx_delay_ms_);
  ESP_LOGCONFIG(TAG, "  Init commands      : %u", (unsigned)init_commands_.size());
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
      while (!tx_queue_.empty()) tx_queue_.pop();
      for (auto *d : devices_) d->on_dequeue();
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
      if (status != ESP_GATT_OK) ESP_LOGW(TAG, "Register notify failed: %d", status);
      break;
    }

    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
      if (param->reg_for_notify.status != ESP_GATT_OK) {
        ESP_LOGW(TAG, "Notify registration failed: %d", param->reg_for_notify.status);
        break;
      }
      last_tx_time_ = millis();
      elm_state_ = ElmState::CONNECTED;
      for (const auto &cmd : init_commands_) tx_queue_.push({cmd, nullptr});
      if (init_commands_.empty()) {
        elm_state_ = ElmState::READY;
        ESP_LOGI(TAG, "No init commands — ELM327 ready");
      } else {
        ESP_LOGI(TAG, "Queued %u init commands", (unsigned) init_commands_.size());
      }
      break;

    case ESP_GATTC_NOTIFY_EVT:
      if (param->notify.handle == rx_char_handle_)
        on_notify(param->notify.value, param->notify.value_len);
      break;

    case ESP_GATTC_WRITE_CHAR_EVT:
      if (param->write.status != ESP_GATT_OK)
        ESP_LOGW(TAG, "Write failed, status=%d", param->write.status);
      break;

    default:
      break;
  }
}

bool BleElm327Component::send_command(const std::string &cmd) {
  if (client_state_ != espbt::ClientState::ESTABLISHED || tx_char_handle_ == 0) return false;
  auto *chr = this->parent_->get_characteristic(service_uuid_, tx_char_uuid_);
  if (chr == nullptr) { ESP_LOGW(TAG, "TX characteristic missing"); return false; }
  chr->write_value(reinterpret_cast<uint8_t *>(const_cast<char *>(cmd.data())), cmd.size(),
                   ESP_GATT_WRITE_TYPE_NO_RSP);
  ESP_LOGD(TAG, ">> %s", cmd.c_str());
  return true;
}

void BleElm327Component::on_notify(const uint8_t *data, uint16_t length) {
  std::string resp(reinterpret_cast<const char *>(data), length);
  process_response(resp);
}

void BleElm327Component::process_response(const std::string &response) {
  ESP_LOGD(TAG, "<< %s", response.c_str());

  if (elm_state_ != ElmState::READY) return;

  const std::string &resp = response;

  // Strip whitespace, CR, LF, '>' — works with both ATS0 (compact) and default (spaced) format
  std::string hex;
  for (char c : resp)
    if (isxdigit(static_cast<unsigned char>(c))) hex += c;

  // Must have at least 4 hex chars (1-byte response code + 1-byte PID/data)
  if (hex.size() < 4) return;

  // Parse consecutive 2-char groups into bytes
  std::vector<uint8_t> bytes;
  for (size_t i = 0; i + 1 < hex.size(); i += 2)
    bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));

  if (bytes.empty()) return;

  // Broadcast to all devices — each checks its own mode+PID and updates if matched
  for (auto *d : devices_) {
    d->on_receive(bytes);
  }
}

}  // namespace ble_elm327
}  // namespace esphome
#endif
