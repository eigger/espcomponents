#pragma once

// Snapshot support is optional. Keeping camera behind this generated define
// lets a normal ws_bridge configuration build without the camera integration.
#ifdef USE_WS_BRIDGE_SNAPSHOT

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#include "esphome/components/camera/camera.h"
#include "esphome/core/component.h"
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ws_protocol.h"
#include "ws_bridge_device.h"

namespace esphome::ws_bridge {

class WsBridgeComponent;

// The ESPHome loop owns camera/control decisions. A dedicated worker owns all
// esp_websocket_client calls, including start/stop/destroy and binary writes.
class WsBridgeSnapshot : public PollingComponent, public camera::CameraListener, public WsBridgeDevice {
 public:
  void set_parent(WsBridgeComponent *parent) { set_ws_bridge_parent(parent); }
  void set_camera(camera::Camera *camera) { camera_ = camera; }
  void set_name(const std::string &name) { name_ = name; }
  void set_platform(const std::string &platform) { platform_ = platform; }
  void set_video(bool video) { video_ = video; }
  void set_video_fps(uint8_t fps) { video_frame_interval_ms_ = 1000 / fps; }
  void set_video_max_bandwidth(uint32_t bytes_per_second) { video_max_bandwidth_ = bytes_per_second; }

  void setup() override;
  void loop() override;
  void update() override;
  void on_camera_image(const std::shared_ptr<camera::CameraImage> &image) override;
  void dump_config() override;
  void ws_bridge_declare() override;
  void ws_bridge_handle_command(const WsCommand &command) override;

 protected:
  enum class WorkType : uint8_t { START, WAKE, EVENT };
  enum class MediaStage : uint8_t { IDLE, WAIT_TRANSPORT, WAIT_AUTH, WAIT_MEDIA_CONNECT, READY };
  struct WorkItem {
    WorkType type;
    esp_websocket_client_handle_t client{nullptr};
    int32_t event_id{0};
    uint8_t opcode{0};
    int payload_len{0};
    int payload_offset{0};
    bool fin{false};
    uint32_t generation{0};
    std::string text;
    std::string session_id;
  };
  struct CallbackContext {
    WsBridgeSnapshot *snapshot;
    uint32_t generation;
  };

  static void media_task_(void *arg);
  static void media_event_(void *arg, esp_event_base_t base, int32_t event_id, void *event_data);
  void worker_loop_();
  bool enqueue_(WorkItem *item);
  void process_item_(WorkItem *item);
  void start_media_(const std::string &session_id);
  void stop_media_();
  void transport_failed_();
  void check_worker_deadline_();
  void handle_media_event_(WorkItem *item);
  void handle_media_message_(const std::string &raw);
  void send_pending_();
  void on_control_connected_();
  void on_control_disconnected_();
  void on_control_message_(const ParsedMessage &message);
  void open_media_();
  void schedule_open_retry_();
  void request_frame_();
  bool media_demand_() const;

  camera::Camera *camera_{nullptr};
  std::string name_;
  std::string platform_{"camera"};
  bool video_{false};
  uint32_t video_frame_interval_ms_{200};
  uint32_t video_max_bandwidth_{256000};
  uint32_t next_video_capture_ms_{0};  // ESPHome loop only
  uint32_t next_video_send_ms_{0};     // worker task only
  QueueHandle_t worker_queue_{nullptr};
  TaskHandle_t worker_task_{nullptr};
  esp_websocket_client_handle_t media_{nullptr};  // worker task only
  std::unique_ptr<CallbackContext> callback_context_;  // worker task only
  std::deque<std::unique_ptr<CallbackContext>> retired_callback_contexts_;
  uint32_t media_generation_{0};   // worker task only, never reset
  uint32_t active_generation_{0};  // worker task only
  std::string worker_session_id_;                 // worker task only
  uint32_t media_connect_id_{0};                  // worker task only
  MediaStage media_stage_{MediaStage::IDLE};      // worker task only
  uint32_t media_stage_started_ms_{0};            // worker task only
  std::string rx_accum_;                          // worker task only
  bool rx_text_frame_{false};                     // worker task only
  bool rx_drop_message_{false};                   // worker task only

  std::mutex frame_mutex_;
  std::shared_ptr<camera::CameraImage> pending_image_;
  bool capture_requested_{false};
  std::atomic<uint8_t> handler_id_{0};
  std::atomic<bool> media_ready_{false};
  std::atomic<bool> needs_media_open_{false};
  std::atomic<bool> stream_requested_{false};
  std::atomic<bool> stop_requested_{false};
  // Event callbacks cannot read worker-only active_generation_. On a full
  // queue they record only their immutable generation; the worker compares it
  // before deciding whether the current client may be stopped.
  std::atomic<uint32_t> terminal_event_generation_{0};
  std::atomic<bool> stopping_{false};
  uint32_t media_open_id_{0};  // ESPHome loop only
  uint32_t media_open_sent_ms_{0};
  uint32_t next_media_open_ms_{0};
  uint32_t media_open_backoff_ms_{2000};
  static constexpr size_t MAX_JPEG_BYTES = 1024 * 1024;
  static constexpr size_t RX_ACCUM_MAX = 16384;
  static constexpr uint32_t MEDIA_OPEN_TIMEOUT_MS = 15000;
  static constexpr uint32_t MEDIA_STAGE_TIMEOUT_MS = 15000;
  static constexpr uint32_t MEDIA_OPEN_BACKOFF_MAX_MS = 30000;
};

}  // namespace esphome::ws_bridge

#endif  // USE_WS_BRIDGE_SNAPSHOT
