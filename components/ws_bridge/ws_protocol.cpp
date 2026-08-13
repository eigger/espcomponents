#include "ws_protocol.h"

namespace esphome {
namespace ws_bridge {

const WsParam *WsCommand::param(const char *key) const {
  for (const auto &p : this->params) {
    if (p.key == key) return &p;
  }
  return nullptr;
}

bool WsCommand::param_float(const char *key, float &out) const {
  const WsParam *p = this->param(key);
  if (p == nullptr || !p->is_number) return false;
  out = p->f;
  return true;
}

bool WsCommand::param_bool(const char *key, bool &out) const {
  const WsParam *p = this->param(key);
  if (p == nullptr || !p->is_bool) return false;
  out = p->b;
  return true;
}

bool WsCommand::param_string(const char *key, std::string &out) const {
  const WsParam *p = this->param(key);
  if (p == nullptr || !p->is_string) return false;
  out = p->s;
  return true;
}

bool WsCommand::param_array(const char *key, std::vector<float> &out, size_t expected_size) const {
  const WsParam *p = this->param(key);
  if (p == nullptr || !p->is_array || p->arr.size() != expected_size) return false;
  out = p->arr;
  return true;
}

ParsedMessage parse_message(const std::string &raw) {
  ParsedMessage msg;
  json::parse_json(raw, [&](JsonObject root) -> bool {
    if (!root["type"].is<const char *>()) return false;
    msg.type = root["type"].as<std::string>();

    if (msg.type == "result") {
      msg.success = root["success"].is<bool>() && root["success"].as<bool>();
      if (!root["id"].isNull()) msg.id = root["id"].as<uint32_t>();
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
        // Copy params out of the JsonDocument — it is destroyed when this
        // callback returns, so JsonObject/JsonVariant must not escape.
        JsonObject params = event["params"].as<JsonObject>();
        if (!params.isNull()) {
          for (JsonPair kv : params) {
            WsParam p;
            p.key = kv.key().c_str();
            JsonVariant v = kv.value();
            if (v.is<JsonArray>()) {
              bool all_numbers = true;
              std::vector<float> nums;
              for (JsonVariant e : v.as<JsonArray>()) {
                // Reject null/string/bool/object/nested-array — as<float>() would
                // quietly yield 0 for those and look like a valid rgb [0,0,...].
                if (!e.is<float>()) {
                  all_numbers = false;
                  break;
                }
                nums.push_back(e.as<float>());
              }
              if (all_numbers) {
                p.is_array = true;
                p.arr = std::move(nums);
              }
            } else if (v.is<const char *>()) {
              p.is_string = true;
              p.s = v.as<std::string>();
            } else if (v.is<bool>()) {
              p.is_bool = true;
              p.b = v.as<bool>();
            } else if (v.is<float>()) {
              // Positive number check — null/object must not become f=0 + success.
              p.is_number = true;
              p.f = v.as<float>();
            }
            msg.command.params.push_back(std::move(p));
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

std::string build_sync(uint32_t id, const std::vector<std::string> &unique_ids) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ws_bridge/sync";
    JsonArray ids = root["unique_ids"].to<JsonArray>();
    for (const auto &unique_id : unique_ids) ids.add(unique_id);
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

std::string build_state_object(uint32_t id, const std::string &unique_id,
                               const std::function<void(JsonObject)> &value_fn) {
  return json::build_json([&](JsonObject root) {
    root["id"] = id;
    root["type"] = "ws_bridge/state";
    JsonArray states = root["states"].to<JsonArray>();
    JsonObject item = states.add<JsonObject>();
    item["unique_id"] = unique_id;
    JsonObject value = item["value"].to<JsonObject>();
    if (value_fn) value_fn(value);
  });
}

}  // namespace ws_bridge
}  // namespace esphome
