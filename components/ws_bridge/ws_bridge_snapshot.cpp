#include "ws_bridge_snapshot.h"

#ifdef USE_WS_BRIDGE_SNAPSHOT

#ifdef USE_WS_BRIDGE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif
#include "esphome/core/log.h"
#include "freertos/task.h"
#include "ws_bridge.h"

namespace esphome::ws_bridge {

static const char *const TAG = "ws_bridge.snapshot";
static constexpr UBaseType_t WORK_QUEUE_LENGTH = 12;

void WsBridgeSnapshot::setup() {
  if (parent_ == nullptr || camera_ == nullptr) {
    ESP_LOGE(TAG, "Snapshot requires ws_bridge and camera");
    mark_failed();
    return;
  }
  worker_queue_ = xQueueCreate(WORK_QUEUE_LENGTH, sizeof(WorkItem *));
  if (worker_queue_ == nullptr ||
      xTaskCreate(media_task_, "ws_bridge_media", 8192, this, tskIDLE_PRIORITY + 1, &worker_task_) != pdPASS) {
    ESP_LOGE(TAG, "Unable to start media worker");
    mark_failed();
    return;
  }
  camera_->add_listener(this);
  parent_->add_on_declare_callback([this]() {
    parent_->send_entity_declare(unique_id_, platform_, name_, "", "", nullptr);
  });
  parent_->add_on_connected_callback([this]() { on_control_connected_(); });
  parent_->add_on_disconnected_callback([this]() { on_control_disconnected_(); });
  parent_->add_message_callback([this](const ParsedMessage &message) { on_control_message_(message); });
}

void WsBridgeSnapshot::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge snapshot:");
  ESP_LOGCONFIG(TAG, "  Entity: %s (%s)", unique_id_.c_str(), platform_.c_str());
  ESP_LOGCONFIG(TAG, "  Capture interval: %u ms", static_cast<unsigned>(get_update_interval()));
}

void WsBridgeSnapshot::loop() {
  if (needs_media_open_.exchange(false, std::memory_order_acq_rel))
    open_media_();
  if (media_open_id_ != 0 && millis() - media_open_sent_ms_ >= MEDIA_OPEN_TIMEOUT_MS) {
    media_open_id_ = 0;
    open_media_();
  }
}

void WsBridgeSnapshot::update() { request_frame_(); }

void WsBridgeSnapshot::on_control_connected_() {
  media_open_id_ = 0;
  media_ready_.store(false, std::memory_order_release);
  handler_id_.store(0, std::memory_order_release);
  enqueue_(new WorkItem{WorkType::STOP});
  open_media_();
}

void WsBridgeSnapshot::on_control_disconnected_() {
  media_open_id_ = 0;
  media_ready_.store(false, std::memory_order_release);
  handler_id_.store(0, std::memory_order_release);
  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    capture_requested_ = false;
    pending_image_.reset();
  }
  enqueue_(new WorkItem{WorkType::STOP});
}

void WsBridgeSnapshot::open_media_() {
  if (parent_ == nullptr || !parent_->is_connected() || media_open_id_ != 0)
    return;
  media_open_id_ = parent_->open_media_session(unique_id_);
  if (media_open_id_ != 0)
    media_open_sent_ms_ = millis();
}

void WsBridgeSnapshot::on_control_message_(const ParsedMessage &message) {
  if (media_open_id_ == 0 || message.type != "result" || message.id != media_open_id_)
    return;
  media_open_id_ = 0;
  if (!message.success || message.session_id.empty()) {
    needs_media_open_.store(true, std::memory_order_release);
    return;
  }
  auto *item = new WorkItem{WorkType::START};
  item->session_id = message.session_id;  // opaque one-use session; never logged
  enqueue_(item);
}

void WsBridgeSnapshot::request_frame_() {
  if (!media_ready_.load(std::memory_order_acquire) || camera_ == nullptr)
    return;
  std::lock_guard<std::mutex> lock(frame_mutex_);
  if (capture_requested_)
    return;
  capture_requested_ = true;
  camera_->request_image(camera::IDLE);
}

void WsBridgeSnapshot::on_camera_image(const std::shared_ptr<camera::CameraImage> &image) {
  if (image == nullptr || !image->was_requested_by(camera::IDLE))
    return;  // do not steal webserver/API camera requests
  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (!capture_requested_)
      return;
    capture_requested_ = false;
    if (image->get_data_length() == 0 || image->get_data_length() > MAX_JPEG_BYTES)
      return;
    // One latest-frame slot bounds RAM and retains the camera buffer until TX.
    pending_image_ = image;
  }
  enqueue_(new WorkItem{WorkType::WAKE});
}

void WsBridgeSnapshot::enqueue_(WorkItem *item) {
  if (item == nullptr)
    return;
  if (worker_queue_ == nullptr || xQueueSend(worker_queue_, &item, 0) != pdPASS) {
    ESP_LOGW(TAG, "Media worker queue full; dropping work item");
    delete item;
  }
}

void WsBridgeSnapshot::media_task_(void *arg) { static_cast<WsBridgeSnapshot *>(arg)->worker_loop_(); }

void WsBridgeSnapshot::worker_loop_() {
  while (true) {
    WorkItem *item = nullptr;
    if (xQueueReceive(worker_queue_, &item, pdMS_TO_TICKS(100)) == pdPASS && item != nullptr) {
      process_item_(item);
      delete item;
    }
    send_pending_();
  }
}

void WsBridgeSnapshot::process_item_(WorkItem *item) {
  switch (item->type) {
    case WorkType::START:
      start_media_(item->session_id);
      break;
    case WorkType::STOP:
      stop_media_();
      break;
    case WorkType::WAKE:
      break;
    case WorkType::EVENT:
      handle_media_event_(item);
      break;
  }
}

void WsBridgeSnapshot::start_media_(const std::string &session_id) {
  stop_media_();
  if (parent_ == nullptr || session_id.empty()) {
    needs_media_open_.store(true, std::memory_order_release);
    return;
  }
  worker_session_id_ = session_id;
  esp_websocket_client_config_t config = {};
  config.host = parent_->get_host().c_str();
  config.port = parent_->get_port();
  config.path = "/api/websocket";
  config.transport = parent_->get_ssl() ? WEBSOCKET_TRANSPORT_OVER_SSL : WEBSOCKET_TRANSPORT_OVER_TCP;
  config.disable_auto_reconnect = true;
  config.network_timeout_ms = 10000;
  config.buffer_size = 1024;
  config.task_stack = parent_->get_ssl() ? 8192 : 4096;
  if (parent_->get_ssl()) {
#ifdef USE_WS_BRIDGE_CERT_BUNDLE
    config.crt_bundle_attach = esp_crt_bundle_attach;
#endif
  }
  media_ = esp_websocket_client_init(&config);
  active_generation_ = ++media_generation_;
  callback_context_ = std::make_unique<CallbackContext>(CallbackContext{this, active_generation_});
  if (media_ == nullptr ||
      esp_websocket_register_events(media_, WEBSOCKET_EVENT_ANY, media_event_, callback_context_.get()) != ESP_OK ||
      esp_websocket_client_start(media_) != ESP_OK) {
    ESP_LOGW(TAG, "Failed to start media WebSocket");
    stop_media_();
    needs_media_open_.store(true, std::memory_order_release);
  }
}

void WsBridgeSnapshot::stop_media_() {
  media_ready_.store(false, std::memory_order_release);
  handler_id_.store(0, std::memory_order_release);
  worker_session_id_.clear();
  media_connect_id_ = 0;
  rx_accum_.clear();
  rx_text_frame_ = false;
  rx_drop_message_ = false;
  if (media_ != nullptr) {
    stopping_.store(true, std::memory_order_release);
    esp_websocket_unregister_events(media_, WEBSOCKET_EVENT_ANY, media_event_);
    esp_websocket_client_stop(media_);
    esp_websocket_client_destroy(media_);
    media_ = nullptr;
    stopping_.store(false, std::memory_order_release);
  }
  active_generation_ = 0;
  // Work items have already copied the generation. Retain a few detached
  // callback contexts while stop/unregister completes, then bound reconnect
  // churn. They are never used to decide ownership after active_generation_
  // has changed.
  if (callback_context_ != nullptr) {
    retired_callback_contexts_.push_back(std::move(callback_context_));
    if (retired_callback_contexts_.size() > 4)
      retired_callback_contexts_.pop_front();
  }
}

void WsBridgeSnapshot::transport_failed_() {
  media_ready_.store(false, std::memory_order_release);
  handler_id_.store(0, std::memory_order_release);
  // A failed partial frame cannot be followed by another binary message: the
  // peer may still await its FIN. Stop/destroy on the owner worker and obtain
  // a fresh one-use HA session through the control channel.
  stop_media_();
  needs_media_open_.store(true, std::memory_order_release);
}

void WsBridgeSnapshot::media_event_(void *arg, esp_event_base_t, int32_t event_id, void *event_data) {
  auto *context = static_cast<CallbackContext *>(arg);
  if (context == nullptr || context->snapshot == nullptr)
    return;
  auto *self = context->snapshot;
  auto *data = static_cast<esp_websocket_event_data_t *>(event_data);
  auto *item = new WorkItem{WorkType::EVENT};
  item->client = data == nullptr ? nullptr : data->client;
  item->event_id = event_id;
  item->generation = context->generation;
  if (event_id == WEBSOCKET_EVENT_DATA && data != nullptr) {
    item->opcode = data->op_code;
    item->payload_len = data->payload_len;
    item->payload_offset = data->payload_offset;
    item->fin = data->fin;
    if (data->data_ptr != nullptr && data->data_len > 0)
      item->text.assign(data->data_ptr, data->data_len);
  }
  self->enqueue_(item);
}

void WsBridgeSnapshot::handle_media_event_(WorkItem *item) {
  if (item->generation != active_generation_ || item->client != media_)
    return;  // stale queued event, including an address-reused client handle
  if (item->event_id == WEBSOCKET_EVENT_DISCONNECTED || item->event_id == WEBSOCKET_EVENT_ERROR ||
      item->event_id == WEBSOCKET_EVENT_CLOSED) {
    if (!stopping_.load(std::memory_order_acquire))
      transport_failed_();
    return;
  }
  if (item->event_id != WEBSOCKET_EVENT_DATA)
    return;
  uint8_t opcode = item->opcode & 0x0F;
  if (opcode != WS_TRANSPORT_OPCODES_TEXT && opcode != WS_TRANSPORT_OPCODES_CONT)
    return;
  // payload_len/offset describe buffer chunks within one WS frame; fin is the
  // separate RFC 6455 signal that the logical TEXT+CONT message is complete.
  bool frame_complete =
      item->payload_len == 0 || item->payload_offset + static_cast<int>(item->text.size()) >= item->payload_len;
  if (rx_drop_message_) {
    if (frame_complete && item->fin) {
      rx_drop_message_ = false;
      rx_text_frame_ = false;
    }
    return;
  }
  if (opcode == WS_TRANSPORT_OPCODES_TEXT)
    rx_text_frame_ = true;
  else if (!rx_text_frame_)
    return;
  if (rx_accum_.size() + item->text.size() > RX_ACCUM_MAX) {
    ESP_LOGW(TAG, "Media response exceeded %u bytes; dropping", static_cast<unsigned>(RX_ACCUM_MAX));
    rx_accum_.clear();
    rx_text_frame_ = false;
    rx_drop_message_ = !(frame_complete && item->fin);
    return;
  }
  rx_accum_.append(item->text);
  if (!frame_complete || !item->fin)
    return;
  rx_text_frame_ = false;
  if (!rx_accum_.empty()) {
    std::string message = std::move(rx_accum_);
    rx_accum_.clear();
    handle_media_message_(message);
  }
}

void WsBridgeSnapshot::handle_media_message_(const std::string &raw) {
  ParsedMessage message = parse_message(raw);
  if (message.type == "auth_required") {
    std::string auth = build_auth(parent_->get_token());
    if (media_ == nullptr || esp_websocket_client_send_text(media_, auth.c_str(), auth.size(), pdMS_TO_TICKS(10000)) < 0)
      transport_failed_();
  } else if (message.type == "auth_ok") {
    media_connect_id_ = 1;
    std::string connect = build_media_connect(media_connect_id_, worker_session_id_);
    if (media_ == nullptr || esp_websocket_client_send_text(media_, connect.c_str(), connect.size(), pdMS_TO_TICKS(10000)) < 0)
      transport_failed_();
  } else if (message.type == "auth_invalid") {
    ESP_LOGE(TAG, "Home Assistant rejected the media connection token");
    transport_failed_();
  } else if (message.type == "result" && message.id == media_connect_id_ && message.success && message.handler_id != 0) {
    handler_id_.store(message.handler_id, std::memory_order_release);
    media_ready_.store(true, std::memory_order_release);
  } else if (message.type == "result" && message.id == media_connect_id_) {
    ESP_LOGW(TAG, "Home Assistant rejected media session");
    transport_failed_();
  }
}

void WsBridgeSnapshot::send_pending_() {
  if (!media_ready_.load(std::memory_order_acquire) || media_ == nullptr || !esp_websocket_client_is_connected(media_))
    return;
  std::shared_ptr<camera::CameraImage> image;
  {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    image = std::move(pending_image_);
  }
  if (image == nullptr)
    return;
  uint8_t handler_id = handler_id_.load(std::memory_order_acquire);
  const char prefix = static_cast<char>(handler_id);
  int first = esp_websocket_client_send_bin_partial(media_, &prefix, 1, pdMS_TO_TICKS(10000));
  int body = first == 1
                 ? esp_websocket_client_send_cont_msg(media_, reinterpret_cast<const char *>(image->get_data_buffer()),
                                                       static_cast<int>(image->get_data_length()), pdMS_TO_TICKS(10000))
                 : -1;
  // send_fin() returns a status/byte count for the empty FIN frame, not the
  // JPEG length. A non-negative result is the documented success condition.
  int finish = body == static_cast<int>(image->get_data_length()) ? esp_websocket_client_send_fin(media_, pdMS_TO_TICKS(10000)) : -1;
  if (first != 1 || body != static_cast<int>(image->get_data_length()) || finish < 0) {
    ESP_LOGW(TAG, "JPEG upload failed; reopening media connection");
    transport_failed_();
  }
}

}  // namespace esphome::ws_bridge

#endif  // USE_WS_BRIDGE_SNAPSHOT
