#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include "esphome/components/json/json_util.h"

namespace esphome {
namespace ws_bridge {

// A command pushed by Home Assistant to a controllable entity
// (switch/number/select/button), per PROTOCOL.md §4.
struct WsCommand {
  std::string unique_id;
  std::string action;  // "turn_on" | "turn_off" | "set_value" | "select_option" | "press"
  bool has_value{false};
  float value_float{0};
  std::string value_string;
};

// Top-level parse result for one incoming WebSocket text frame. Only the
// fields this client cares about are extracted (the JsonDocument backing the
// parse is gone once parse_message() returns, so everything needed must be
// copied out as plain C++ values here).
struct ParsedMessage {
  std::string type;  // "auth_required" | "auth_ok" | "auth_invalid" | "result" | "event" | "" (unrecognized)
  bool success{false};  // for "result"
  WsCommand command;     // populated when type == "event" and event.kind == "command"
};

ParsedMessage parse_message(const std::string &raw);

// Outgoing message builders (HA websocket auth messages have no "id"; every
// ws_bridge/* command does).
std::string build_auth(const std::string &access_token);
std::string build_connect(uint32_t id, const std::string &gateway_id, const std::string &name,
                          bool keep_last_state_on_disconnect);

// Application-level keepalive (HA's standard websocket_api "ping"/"pong"
// commands) — used to actively detect a dead connection that the transport
// layer itself doesn't notice (e.g. HA process killed without a clean WS
// close, so the socket never sees a FIN/RST).
std::string build_ping(uint32_t id);

// `extra` (may be empty) is called with the message's root JsonObject to add
// platform-specific declare fields (device_class, options, min/max/step, ...).
std::string build_entity_declare(uint32_t id, const std::string &unique_id, const std::string &platform,
                                 const std::string &name, const std::string &device_id,
                                 const std::string &device_name,
                                 const std::function<void(JsonObject)> &extra);

std::string build_state_float(uint32_t id, const std::string &unique_id, float value);
std::string build_state_bool(uint32_t id, const std::string &unique_id, bool value);
std::string build_state_string(uint32_t id, const std::string &unique_id, const std::string &value);

}  // namespace ws_bridge
}  // namespace esphome
