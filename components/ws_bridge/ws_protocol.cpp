#include "ws_protocol.h"

namespace esphome {
namespace ws_bridge {

ParsedMessage parse_message(const std::string &raw) {
  ParsedMessage msg;
  json::parse_json(raw, [&](JsonObject root) -> bool {
    if (!root["type"].is<const char *>()) return false;
    msg.type = root["type"].as<std::string>();

    if (msg.type == "result") {
      msg.success = root["success"].is<bool>() && root["success"].as<bool>();
      return true;
    }

    if (msg.type == "event") {
      JsonObject event = root["event"].as<JsonObject>();
      if (!event.isNull() && event["kind"].is<const char *>() &&
          std::string(event["kind"].as<const char *>()) == "command") {
        if (event["unique_id"].is<const char *>()) msg.command.unique_id = event["unique_id"].as<std::string>();
        if (event["action"].is<const char *>()) msg.command.action = event["action"].as<std::string>();
        if (!event["value"].isNull()) {
          msg.command.has_value = true;
          if (event["value"].is<const char *>()) {
            msg.command.value_string = event["value"].as<std::string>();
          } else {
            msg.command.value_float = event["value"].as<float>();
          }
        }
      }
      return true;
    }

    return true;  // auth_required / auth_ok / auth_invalid: only msg.type is needed
  });
  return msg;
}

std::string build_auth(const std::string &access_token) {
  return json::build_json([&](JsonObject root) {
    root["type"] = "auth";
    root["access_token"] = access_token;
  });
}

std::string build_connect(uint32_t id, const std::string &gateway_id, const std::string &name,
                          bool keep_last_state_on_disconnect) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ws_bridge/connect";
    root["gateway_id"] = gateway_id;
    if (!name.empty()) root["name"] = name;
    root["keep_last_state_on_disconnect"] = keep_last_state_on_disconnect;
  });
}

std::string build_entity_declare(uint32_t id, const std::string &unique_id, const std::string &platform,
                                 const std::string &name, const std::string &device_id,
                                 const std::string &device_name,
                                 const std::function<void(JsonObject)> &extra) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ws_bridge/entity";
    root["unique_id"] = unique_id;
    root["platform"] = platform;
    root["name"] = name;
    if (!device_id.empty()) {
      JsonObject dev = root["device"].to<JsonObject>();
      dev["id"] = device_id;
      if (!device_name.empty()) dev["name"] = device_name;
    }
    if (extra) extra(root);
  });
}

std::string build_ping(uint32_t id) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ping";
  });
}

std::string build_state_float(uint32_t id, const std::string &unique_id, float value) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ws_bridge/state";
    JsonArray states = root["states"].to<JsonArray>();
    JsonObject item = states.add<JsonObject>();
    item["unique_id"] = unique_id;
    item["value"] = value;
  });
}

std::string build_state_bool(uint32_t id, const std::string &unique_id, bool value) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ws_bridge/state";
    JsonArray states = root["states"].to<JsonArray>();
    JsonObject item = states.add<JsonObject>();
    item["unique_id"] = unique_id;
    item["value"] = value;
  });
}

std::string build_state_string(uint32_t id, const std::string &unique_id, const std::string &value) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ws_bridge/state";
    JsonArray states = root["states"].to<JsonArray>();
    JsonObject item = states.add<JsonObject>();
    item["unique_id"] = unique_id;
    item["value"] = value;
  });
}

}  // namespace ws_bridge
}  // namespace esphome
