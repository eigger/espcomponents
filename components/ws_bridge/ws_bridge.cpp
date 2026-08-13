#include "ws_bridge.h"
#include <algorithm>
#include "esp_crt_bundle.h"
#include "esp_transport_ws.h"
#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge";

void WsBridgeComponent::setup() {
  esp_websocket_client_config_t config = {};
  config.host = this->host_.c_str();
  config.port = this->port_;
  config.path = "/api/websocket";
  config.transport = this->ssl_ ? WEBSOCKET_TRANSPORT_OVER_SSL : WEBSOCKET_TRANSPORT_OVER_TCP;
  if (this->ssl_) {
    config.crt_bundle_attach = esp_crt_bundle_attach;
  }
  config.disable_auto_reconnect = false;
  config.reconnect_timeout_ms = 10000;
  config.network_timeout_ms = 10000;
  // Pin the RX/TX buffer so a future library default change cannot silently
  // alter chunking (see rx_drop_message_ — one TEXT frame is split into
  // buffer_size DATA events).
  config.buffer_size = 1024;
  // Default task stack is 4KB. TLS handshake + crt bundle + our handler's
  // std::string work needs more headroom (Nabu Casa / reverse-proxy chains).
  config.task_stack = this->ssl_ ? 8192 : 4096;

  this->client_ = esp_websocket_client_init(&config);
  if (this->client_ == nullptr) {
    ESP_LOGE(TAG, "Failed to init WebSocket client");
    this->mark_failed();
    return;
  }
  esp_websocket_register_events(this->client_, WEBSOCKET_EVENT_ANY, WsBridgeComponent::ws_event_handler_, this);
  this->reconnect_backoff_ms_ = this->reconnect_backoff_base_();
}

void WsBridgeComponent::loop() {
  if (!this->started_) {
    if (!network::is_connected()) return;
    uint32_t now = millis();
    if (this->last_reconnect_attempt_ms_ != 0 &&
        now - this->last_reconnect_attempt_ms_ < this->reconnect_backoff_ms_)
      return;
    esp_err_t err = esp_websocket_client_start(this->client_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "esp_websocket_client_start failed: %d", err);
      this->last_reconnect_attempt_ms_ = now;
      this->reconnect_backoff_ms_ =
          std::min(std::max(this->reconnect_backoff_base_(), this->reconnect_backoff_ms_ * 2),
                   this->reconnect_retry_ms_);
      return;
    }
    this->started_ = true;
    this->last_reconnect_attempt_ms_ = now;
    this->reconnect_backoff_ms_ = this->reconnect_backoff_base_();
  }

  WsEvent *event;
  while ((event = this->event_queue_.pop()) != nullptr) {
    this->handle_event_(*event);
    this->event_pool_.release(event);
  }

  // stop()/start() runs on reconnect_task_; do not send or poke liveness
  // (including backoff) while that is in flight.
  if (this->reconnect_task_busy_.load(std::memory_order_acquire)) {
    if (!this->reconnect_stuck_warned_ &&
        millis() - this->reconnect_task_started_ms_ > RECONNECT_STUCK_WARN_MS) {
      ESP_LOGW(TAG, "Reconnect task still running after %u ms (stop() waiting on DNS/TLS?)",
               static_cast<unsigned>(RECONNECT_STUCK_WARN_MS));
      this->reconnect_stuck_warned_ = true;
    }
    return;
  }

  this->drain_tx_queue_();
  this->progress_declare_();
  // Drain again so frames enqueued by this declare slice can leave before
  // sync is considered, and so a stalled peer is retried every loop.
  this->drain_tx_queue_();
  this->maybe_flush_sync_();
  this->check_liveness_();
}

// Forces a fresh connection attempt, bypassing whatever esp_websocket_client
// thinks it's doing internally. Used both when we've actively determined the
// current connection is dead (ping/pong) and when we've simply been
// disconnected too long (see check_liveness_) — the latter matters because
// the client's own auto-reconnect (disable_auto_reconnect = false) has been
// observed to stop making progress after a prolonged outage (e.g. Home
// Assistant itself restarting, which can take well over a minute), with no
// further event ever firing for us to react to. Without this backstop that
// required power-cycling the ESP to recover.
void WsBridgeComponent::force_reconnect_() {
  this->last_reconnect_attempt_ms_ = millis();
  this->set_state_(WS_BRIDGE_DISCONNECTED);
  this->clear_tx_queue_();
  this->declare_in_progress_ = false;
  this->sync_flush_pending_ = false;
  this->collecting_declared_ids_ = false;
  // esp_websocket_client_stop() waits with portMAX_DELAY for the WS task to
  // exit. That task may be stuck in DNS (~14s) or TCP/TLS connect
  // (network_timeout_ms = 10s) — exactly when we force a reconnect. Doing
  // this on the ESPHome loop task trips the 5s task WDT (PANIC).
  bool expected = false;
  if (!this->reconnect_task_busy_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    return;
  this->reconnect_task_started_ms_ = millis();
  this->reconnect_stuck_warned_ = false;
  // Same work as the WS client task (TLS close_notify, inline event dispatch)
  // can run here; 4KB matches the non-TLS WS stack and is only temporary.
  BaseType_t ok = xTaskCreate(&WsBridgeComponent::reconnect_task_, "ws_bridge_rc", 4096, this, 5, nullptr);
  if (ok != pdPASS) {
    ESP_LOGE(TAG, "Failed to create reconnect task");
    this->reconnect_task_busy_.store(false, std::memory_order_release);
  }
}

void WsBridgeComponent::reconnect_task_(void *arg) {
  auto *self = static_cast<WsBridgeComponent *>(arg);
  if (self->client_ != nullptr) {
    esp_websocket_client_stop(self->client_);
    esp_err_t err = esp_websocket_client_start(self->client_);
    if (err != ESP_OK)
      ESP_LOGW(TAG, "esp_websocket_client_start failed after stop: %d", err);
  }
  self->reconnect_task_busy_.store(false, std::memory_order_release);
  vTaskDelete(nullptr);
}

uint32_t WsBridgeComponent::reconnect_backoff_base_() const {
  return std::min(RECONNECT_BACKOFF_BASE_MS, this->reconnect_retry_ms_);
}

std::string WsBridgeComponent::effective_sw_version_() {
  if (!this->sw_version_.empty()) return this->sw_version_;
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 1, 0)
  char build_time[Application::BUILD_TIME_STR_SIZE];
  App.get_build_time_string(build_time);
  return std::string(ESPHOME_VERSION) + " (" + build_time + ")";
#else
  return std::string(ESPHOME_VERSION) + " (" + App.get_compilation_time() + ")";
#endif
}

void WsBridgeComponent::send_connect_(uint32_t id) {
  this->send_raw_(build_connect(id, this->gateway_id_, this->gateway_name_,
                                this->keep_last_state_on_disconnect_, this->effective_sw_version_(),
                                this->manufacturer_, this->model_, this->hw_version_));
}

// Actively probes the connection with HA's standard "ping"/"pong" websocket_api
// commands. Needed because a dead peer (e.g. HA killed without a clean WS
// close — no FIN/RST ever reaches the socket) can otherwise leave the
// underlying esp_websocket_client believing it's still connected indefinitely,
// so is_connected() alone never reports the failure and auto-reconnect never
// kicks in.
void WsBridgeComponent::check_liveness_() {
  uint32_t now = millis();
  if (this->reconnect_task_busy_.load(std::memory_order_acquire))
    return;
  if (!this->is_connected()) {
    if (now - this->last_reconnect_attempt_ms_ > this->reconnect_backoff_ms_) {
      ESP_LOGW(TAG, "Still disconnected after %u ms — forcing a fresh connection attempt",
               static_cast<unsigned>(this->reconnect_backoff_ms_));
      this->force_reconnect_();
      this->reconnect_backoff_ms_ = std::min(this->reconnect_backoff_ms_ * 2, this->reconnect_retry_ms_);
    }
    return;
  }
  if (this->ping_outstanding_) {
    if (now - this->last_ping_sent_ms_ > this->pong_timeout_ms_) {
      ESP_LOGW(TAG, "No pong received within %u ms — forcing reconnect",
               static_cast<unsigned>(this->pong_timeout_ms_));
      this->ping_outstanding_ = false;
      this->reconnect_backoff_ms_ = this->reconnect_backoff_base_();
      this->force_reconnect_();
    }
    return;
  }
  // A periodic re-announce's ws_bridge/connect went unanswered — if HA (not
  // just its ws_bridge integration) is unresponsive, re-sending again on the
  // next interval would just repeat the same no-op forever, so treat this
  // like any other dead-connection signal.
  if (this->awaiting_connect_result_) {
    if (now - this->connect_sent_ms_ > this->pong_timeout_ms_) {
      ESP_LOGW(TAG, "No result for ws_bridge/connect within %u ms — forcing reconnect",
               static_cast<unsigned>(this->pong_timeout_ms_));
      this->awaiting_connect_result_ = false;
      this->reconnect_backoff_ms_ = this->reconnect_backoff_base_();
      this->force_reconnect_();
    }
    return;
  }
  // See the comment on reannounce_interval_ms_: this doesn't wait for any
  // detected failure, it just periodically re-establishes our registration
  // in case the HA-side integration silently lost track of us while the
  // transport itself (and ping/pong) stayed healthy.
  // Skip while a declare pass or TX backlog is still draining — stacking
  // another full redeclare on a congested socket aborts the connection
  // (esp_websocket_client treats timed-out 0-byte writes as fatal).
  if (!this->declare_in_progress_ && this->tx_queue_.empty() &&
      now - this->last_reannounce_ms_ > this->reannounce_interval_ms_) {
    ESP_LOGD(TAG, "Periodic re-announce: resending connect + entity declarations");
    uint32_t connect_id = this->next_id_();
    this->send_connect_(connect_id);
    this->last_connect_msg_id_ = connect_id;
    this->awaiting_connect_result_ = true;
    this->connect_sent_ms_ = now;
    // Re-announce only re-declares — no on_connected: and no sync (stale
    // entities are not urgent enough to scan HA's registry every interval).
    this->start_declare_pass_(/*run_connected_and_sync=*/false);
    this->last_reannounce_ms_ = now;
  }
  if (now - this->last_ping_sent_ms_ > this->ping_interval_ms_) {
    this->send_raw_(build_ping(this->next_id_()));
    this->ping_outstanding_ = true;
    this->last_ping_sent_ms_ = now;
  }
}

void WsBridgeComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge:");
  ESP_LOGCONFIG(TAG, "  Server: %s://%s:%u/api/websocket", this->ssl_ ? "wss" : "ws", this->host_.c_str(),
                this->port_);
  ESP_LOGCONFIG(TAG, "  Gateway ID: %s", this->gateway_id_.c_str());
  ESP_LOGCONFIG(TAG, "  Software version: %s", this->effective_sw_version_().c_str());
  if (!this->manufacturer_.empty())
    ESP_LOGCONFIG(TAG, "  Manufacturer: %s", this->manufacturer_.c_str());
  if (!this->model_.empty())
    ESP_LOGCONFIG(TAG, "  Model: %s", this->model_.c_str());
  if (!this->hw_version_.empty())
    ESP_LOGCONFIG(TAG, "  Hardware version: %s", this->hw_version_.c_str());
  ESP_LOGCONFIG(TAG, "  Keep last state on disconnect: %s", YESNO(this->keep_last_state_on_disconnect_));
  ESP_LOGCONFIG(TAG, "  Ping interval: %u ms", static_cast<unsigned>(this->ping_interval_ms_));
  ESP_LOGCONFIG(TAG, "  Pong timeout: %u ms", static_cast<unsigned>(this->pong_timeout_ms_));
  ESP_LOGCONFIG(TAG, "  Reconnect backoff cap: %u ms", static_cast<unsigned>(this->reconnect_retry_ms_));
  ESP_LOGCONFIG(TAG, "  Re-announce interval: %u ms", static_cast<unsigned>(this->reannounce_interval_ms_));

  for (size_t i = 0; i < this->devices_.size(); i++) {
    const std::string &uid = this->devices_[i]->get_ws_bridge_unique_id();
    for (size_t j = i + 1; j < this->devices_.size(); j++) {
      if (uid == this->devices_[j]->get_ws_bridge_unique_id()) {
        ESP_LOGW(TAG, "Duplicate unique_id '%s' — Home Assistant will treat these as one entity", uid.c_str());
      }
    }
    const EntityBase *src = this->devices_[i]->get_ws_bridge_source();
    if (src == nullptr)
      continue;
    for (size_t j = i + 1; j < this->devices_.size(); j++) {
      if (this->devices_[j]->get_ws_bridge_source() != src)
        continue;
      ESP_LOGW(TAG,
               "Entity '%s' is exposed twice (unique_ids '%s' and '%s') — pick either "
               "`platform: ws_bridge` wrapping (*_id:) or `entities:`, not both",
               src->get_name().str().c_str(), uid.c_str(),
               this->devices_[j]->get_ws_bridge_unique_id().c_str());
    }
  }
}

// May be called from either the main loop task or the esp_websocket_client
// task (see ws_event_handler_), so this must be a single atomic RMW rather
// than a load-compare-store.
void WsBridgeComponent::set_state_(WsBridgeState s) {
  WsBridgeState old = this->state_.exchange(s, std::memory_order_acq_rel);
  if (old != s) {
    ESP_LOGD(TAG, "state %d -> %d", old, s);
  }
}

// Typically runs on the esp_websocket_client task, but can also run on the
// ESPHome loop task (or reconnect_task_) because the library creates its
// event loop with task_name = NULL and dispatches inline after send failures
// / stop/start. Stay fast: reassemble fragments into rx_accum_ and hand off
// complete events through the lock-free queue. Mutual exclusion with send
// currently depends on esp_websocket_client's client->lock.
void WsBridgeComponent::ws_event_handler_(void *handler_args, esp_event_base_t base, int32_t event_id,
                                          void *event_data) {
  auto *self = static_cast<WsBridgeComponent *>(handler_args);
  auto *data = static_cast<esp_websocket_event_data_t *>(event_data);
  auto ws_event_id = static_cast<esp_websocket_event_id_t>(event_id);

  bool was_connected = false;

  // A (re)connect or drop always starts a fresh message stream: discard any
  // partial fragment left over from a message that never completed (e.g. the
  // socket dropped mid-fragment), so it can't get concatenated with data from
  // a later connection.
  //
  // The state_ transition itself also happens right here, unconditionally,
  // rather than being deferred to loop()'s consumption of the queued event.
  // event_queue_ is bounded (EVENT_QUEUE_SIZE) and silently drops events when
  // full (see below) — if a dropped event were the only place a disconnect
  // got recorded, state_ could stay stuck at WS_BRIDGE_CONNECTED across a
  // reconnect. is_connected() would then still report true on the fresh,
  // not-yet-authenticated socket, so a state push could go out before HA even
  // sends auth_required, which HA's auth handler correctly rejects. Doing the
  // transition here means it can never be lost to a full queue; only the
  // (non-critical) log line / callback in handle_event_ can be.
  if (ws_event_id == WEBSOCKET_EVENT_CONNECTED) {
    self->rx_accum_.clear();
    self->rx_text_frame_ = false;
    self->rx_drop_message_ = false;
    self->set_state_(WS_BRIDGE_WAIT_AUTH_REQUIRED);
  } else if (ws_event_id == WEBSOCKET_EVENT_DISCONNECTED || ws_event_id == WEBSOCKET_EVENT_ERROR ||
             ws_event_id == WEBSOCKET_EVENT_CLOSED) {
    self->rx_accum_.clear();
    self->rx_text_frame_ = false;
    self->rx_drop_message_ = false;
    was_connected = (self->state_.exchange(WS_BRIDGE_DISCONNECTED, std::memory_order_acq_rel) == WS_BRIDGE_CONNECTED);
  }

  if (ws_event_id == WEBSOCKET_EVENT_DATA) {
    uint8_t opcode = static_cast<uint8_t>(data->op_code) & 0x0F;
    // Control/binary frames are not part of reassembly — they must not
    // complete (or cancel) an in-progress oversized drop either.
    if (opcode != WS_TRANSPORT_OPCODES_TEXT && opcode != WS_TRANSPORT_OPCODES_CONT)
      return;
    if (self->rx_drop_message_) {
      // Remaining chunks of an oversized frame (WS CONT *or* buffer_size
      // splits of a single TEXT opcode) must not be reassembled.
      bool drop_complete =
          data->payload_len == 0 || (data->payload_offset + data->data_len >= data->payload_len);
      if (drop_complete) {
        self->rx_drop_message_ = false;
        self->rx_text_frame_ = false;
      }
      return;
    }
    if (opcode == WS_TRANSPORT_OPCODES_TEXT) {
      self->rx_text_frame_ = true;
    } else if (!self->rx_text_frame_) {
      return;  // continuation of a binary (or unknown) message
    }
    if (data->data_len > 0) {
      if (self->rx_accum_.size() + static_cast<size_t>(data->data_len) > RX_ACCUM_MAX) {
        ESP_LOGW(TAG, "RX message exceeded %u bytes — dropping", (unsigned) RX_ACCUM_MAX);
        self->rx_accum_.clear();
        self->rx_accum_.shrink_to_fit();
        self->rx_text_frame_ = false;
        bool complete =
            data->payload_len == 0 || (data->payload_offset + data->data_len >= data->payload_len);
        // Only hold the drop across further chunks of *this* frame. If this
        // chunk already finished the payload, the next DATA is a new message.
        self->rx_drop_message_ = !complete;
        return;
      }
      self->rx_accum_.append(data->data_ptr, data->data_len);
    }
    bool complete = data->payload_len == 0 || (data->payload_offset + data->data_len >= data->payload_len);
    if (!complete) return;  // wait for more fragments
    self->rx_text_frame_ = false;
    if (self->rx_accum_.empty())
      return;  // empty payload would just consume a pool slot
  }

  WsEvent *event = self->event_pool_.allocate();
  if (event == nullptr) {
    // Dropping a completed DATA message must also drop rx_accum_. Leaving it
    // would concatenate the next frame onto the abandoned payload and poison
    // every subsequent JSON message until the next connect/disconnect.
    self->rx_accum_.clear();
    self->rx_text_frame_ = false;
    return;
  }
  event->event_id = ws_event_id;
  event->was_connected = was_connected;
  if (ws_event_id == WEBSOCKET_EVENT_DATA) {
    event->data = std::move(self->rx_accum_);
    self->rx_accum_.clear();
  }
  if (!self->event_queue_.push(event)) self->event_pool_.release(event);
}

void WsBridgeComponent::handle_event_(const WsEvent &event) {
  switch (event.event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
      // state_ is already WS_BRIDGE_WAIT_AUTH_REQUIRED — set by
      // ws_event_handler_ itself, see the comment there.
      ESP_LOGI(TAG, "WebSocket transport connected");
      break;
    case WEBSOCKET_EVENT_DISCONNECTED:
    case WEBSOCKET_EVENT_ERROR:
    case WEBSOCKET_EVENT_CLOSED:
      // state_ is already WS_BRIDGE_DISCONNECTED — set by ws_event_handler_
      // itself. event.was_connected is a snapshot taken at that time; by now
      // is_connected() would always read false, so it can't be used here to
      // tell whether this is a real transition.
      if (event.was_connected) {
        ESP_LOGW(TAG, "WebSocket disconnected");
        // Anchor the reconnect backoff to this disconnect, not to whatever
        // earlier attempt last_reconnect_attempt_ms_ still held — otherwise
        // a connection that drops shortly after connecting could sit idle
        // for however much of reconnect_retry_ms_ was already "used up"
        // since that earlier attempt, instead of retrying promptly. See the
        // comment on reconnect_backoff_ms_ in ws_bridge.h.
        this->last_reconnect_attempt_ms_ = millis();
        this->reconnect_backoff_ms_ = this->reconnect_backoff_base_();
        this->clear_tx_queue_();
        this->declare_in_progress_ = false;
        this->sync_flush_pending_ = false;
        this->collecting_declared_ids_ = false;
        this->disconnected_cb_.call();
      }
      break;
    case WEBSOCKET_EVENT_DATA:
      this->handle_message_(event.data);
      break;
    default:
      break;
  }
}

void WsBridgeComponent::handle_message_(const std::string &raw) {
  ParsedMessage msg = parse_message(raw);

  if (msg.type == "auth_required") {
    this->send_raw_(build_auth(this->token_));
    this->set_state_(WS_BRIDGE_WAIT_AUTH_OK);
  } else if (msg.type == "auth_ok") {
    this->send_connect_(this->next_id_());
    this->set_state_(WS_BRIDGE_CONNECTED);
    this->ping_outstanding_ = false;
    this->reconnect_backoff_ms_ = this->reconnect_backoff_base_();
    this->last_ping_sent_ms_ = millis();
    this->last_reannounce_ms_ = this->last_ping_sent_ms_;
    // Platform declares + on_declare:/on_connected: + sync are paced across
    // loop() iterations — see start_declare_pass_().
    this->start_declare_pass_(/*run_connected_and_sync=*/true);
  } else if (msg.type == "auth_invalid") {
    ESP_LOGE(TAG, "Home Assistant rejected the access token");
  } else if (msg.type == "pong") {
    this->ping_outstanding_ = false;
  } else if (msg.type == "event") {
    if (!msg.command.unique_id.empty()) this->route_command_(msg.command);
  } else if (msg.type == "result") {
    // Only the periodic re-announce's ws_bridge/connect is tracked (see
    // check_liveness_); every other ws_bridge/* result is fire-and-forget.
    if (this->awaiting_connect_result_ && msg.id == this->last_connect_msg_id_) {
      this->awaiting_connect_result_ = false;
    }
  }
}

void WsBridgeComponent::route_command_(const WsCommand &command) {
  for (auto *device : this->devices_) {
    if (device->get_ws_bridge_unique_id() == command.unique_id) {
      device->ws_bridge_handle_command(command);
      return;
    }
  }
  ESP_LOGW(TAG, "Command for unknown unique_id '%s'", command.unique_id.c_str());
}

void WsBridgeComponent::start_declare_pass_(bool run_connected_and_sync) {
  this->declare_in_progress_ = true;
  this->declare_device_index_ = 0;
  this->declare_run_connected_ = run_connected_and_sync;
  if (run_connected_and_sync) {
    // Collect across the paced declare pass and on_connected:, so a lambda
    // that declares from either trigger is counted before sync is sent.
    this->collecting_declared_ids_ = this->sync_entities_;
    this->declared_ids_.clear();
    this->sync_flush_pending_ = this->sync_entities_;
  } else {
    this->collecting_declared_ids_ = false;
    this->sync_flush_pending_ = false;
  }
}

void WsBridgeComponent::progress_declare_() {
  if (this->declare_in_progress_) {
    size_t n = 0;
    while (this->declare_device_index_ < this->devices_.size() && n < DECLARE_DEVICES_PER_LOOP) {
      this->devices_[this->declare_device_index_++]->ws_bridge_declare();
      n++;
    }
    if (this->declare_device_index_ < this->devices_.size())
      return;

    // Manual (lambda-built) declarations piggyback here rather than on
    // on_connected alone, so they're re-sent by the periodic re-announce too.
    this->declare_cb_.call();
    if (this->declare_run_connected_) {
      this->connected_cb_.call();
      this->declare_run_connected_ = false;
    }
    this->declare_in_progress_ = false;
  }
  this->maybe_flush_sync_();
}

void WsBridgeComponent::maybe_flush_sync_() {
  if (!this->sync_flush_pending_)
    return;
  if (this->declare_in_progress_ || !this->tx_queue_.empty())
    return;
  this->flush_sync_();
  this->sync_flush_pending_ = false;
}

void WsBridgeComponent::flush_sync_() {
  this->collecting_declared_ids_ = false;
  if (!this->sync_entities_)
    return;
  // HA rejects an empty list (it would mean "delete everything"), and a
  // gateway that declared nothing has nothing to reconcile against anyway.
  if (this->declared_ids_.empty()) {
    ESP_LOGW(TAG, "sync_entities is on but nothing was declared — skipping ws_bridge/sync");
    return;
  }
  ESP_LOGD(TAG, "Syncing %u declared entities with HA", (unsigned) this->declared_ids_.size());
  this->send_raw_(build_sync(this->next_id_(), this->declared_ids_));
  this->declared_ids_.clear();
  this->declared_ids_.shrink_to_fit();
}

void WsBridgeComponent::clear_tx_queue_() { this->tx_queue_.clear(); }

void WsBridgeComponent::drain_tx_queue_() {
  if (this->client_ == nullptr || !esp_websocket_client_is_connected(this->client_))
    return;
  size_t sent = 0;
  while (!this->tx_queue_.empty() && sent < TX_PER_LOOP) {
    TxItem &item = this->tx_queue_.front();
    int r = esp_websocket_client_send_text(this->client_, item.msg.c_str(), item.msg.size(),
                                           pdMS_TO_TICKS(TX_SEND_TIMEOUT_MS));
    if (r < 0) {
      // Library already aborted the socket on a 0-byte/failed write — drop the
      // backlog rather than retrying into a dead connection.
      this->clear_tx_queue_();
      break;
    }
    if (this->collecting_declared_ids_ && !item.sync_declare_uid.empty())
      this->declared_ids_.push_back(item.sync_declare_uid);
    this->tx_queue_.pop_front();
    sent++;
  }
}

bool WsBridgeComponent::send_raw_(const std::string &msg, const std::string &sync_declare_uid) {
  if (this->client_ == nullptr || !esp_websocket_client_is_connected(this->client_))
    return false;
  // Keep FIFO: a queued v1 must leave before a later v2. Bypass the queue only
  // when it is empty. Timeout must be long enough that a full TCP window can
  // drain — a short/0 wait makes esp_websocket_client abort the connection.
  if (this->tx_queue_.empty()) {
    int r = esp_websocket_client_send_text(this->client_, msg.c_str(), msg.size(),
                                           pdMS_TO_TICKS(TX_SEND_TIMEOUT_MS));
    if (r >= 0) {
      if (this->collecting_declared_ids_ && !sync_declare_uid.empty())
        this->declared_ids_.push_back(sync_declare_uid);
      return true;
    }
    // Send aborted the socket; do not queue onto a dead link.
    return false;
  }
  if (this->tx_queue_.size() >= TX_QUEUE_MAX) {
    ESP_LOGW(TAG, "TX queue full (%u) — dropping outbound message", (unsigned) TX_QUEUE_MAX);
    return false;
  }
  this->tx_queue_.push_back(TxItem{msg, sync_declare_uid});
  return true;
}

void WsBridgeComponent::send_entity_declare(const std::string &unique_id, const std::string &platform,
                                            const std::string &name, const std::string &device_id,
                                            const std::string &device_name,
                                            const std::function<void(JsonObject)> &extra) {
  if (!this->is_connected())
    return;
  // Every declaration funnels through here — registered platform entities and
  // hand-built lambda ones alike — so this is the one place that sees the full
  // set for ws_bridge/sync. unique_id is only recorded after the frame is
  // actually written (or when a queued frame is later drained).
  const std::string sync_uid = this->collecting_declared_ids_ ? unique_id : "";
  this->send_raw_(build_entity_declare(this->next_id_(), unique_id, platform, name, device_id, device_name, extra),
                  sync_uid);
}

void WsBridgeComponent::send_state_float(const std::string &unique_id, float value) {
  if (!this->is_connected())
    return;
  this->send_raw_(build_state_float(this->next_id_(), unique_id, value));
}

void WsBridgeComponent::send_state_bool(const std::string &unique_id, bool value) {
  if (!this->is_connected())
    return;
  this->send_raw_(build_state_bool(this->next_id_(), unique_id, value));
}

void WsBridgeComponent::send_state_string(const std::string &unique_id, const std::string &value) {
  if (!this->is_connected())
    return;
  this->send_raw_(build_state_string(this->next_id_(), unique_id, value));
}

void WsBridgeComponent::send_state_object(const std::string &unique_id,
                                          const std::function<void(JsonObject)> &value_fn) {
  if (!this->is_connected())
    return;
  this->send_raw_(build_state_object(this->next_id_(), unique_id, value_fn));
}

}  // namespace ws_bridge
}  // namespace esphome
