#include "ws_bridge_lock.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.lock";

void WsBridgeLock::setup() {
  if (this->source_ == nullptr) {
    // Standalone, so this entity's own traits are what get declared. Offer the
    // full surface — same reasoning as WsBridgeCover::get_traits() advertising
    // position and stop: there is no hardware here to constrain it, and open()
    // refuses outright unless supports_open is set. LockCall::validate_() drops
    // any state outside the supported mask, so OPEN has to be added for
    // lambda-driven `lock.open` on this entity to survive.
    this->traits.set_supports_open(true);
    this->traits.add_supported_state(lock::LOCK_STATE_OPEN);
  }
  ws_subscribe_lock(this, this->source_ != nullptr ? this->source_ : this);
}

void WsBridgeLock::dump_config() {
  LOG_LOCK("", "WS Bridge Lock", this);
  if (!this->code_format_.empty())
    ESP_LOGCONFIG(TAG, "  Code Format: '%s'", this->code_format_.c_str());
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

// Only reached when NOT wrapping — see WsBridgeSwitch::write_state().
void WsBridgeLock::control(const lock::LockCall &call) {
  if (call.get_state().has_value())
    this->publish_state(*call.get_state());
}

// Lock::open() routes here instead of through control(). The base
// implementation falls back to unlock(); report the unlatched state HA asked
// for instead.
void WsBridgeLock::open_latch() { this->publish_state(lock::LOCK_STATE_OPEN); }

void WsBridgeLock::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_lock(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeLock::ws_bridge_declare() {
  lock::Lock &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_lock(this, src, this, ws_ha_name(src, own_name, this->unique_id_), this->code_format_);
  ws_push_state_lock(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
