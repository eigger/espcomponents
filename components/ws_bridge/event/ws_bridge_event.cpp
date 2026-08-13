#include "ws_bridge_event.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.event";

void WsBridgeEvent::setup() {
  if (this->source_ != nullptr) {
    // `event_types:` is optional when wrapping, so this entity's own list may
    // be empty. Adopt the source's — the declaration reads from the source
    // either way, but leaving this entity with no types would make it invalid
    // for everything else that lists ESPHome entities (API, web server).
    this->set_event_types(this->source_->get_event_types());
  }
  ws_subscribe_event(this, this->source_ != nullptr ? this->source_ : this);
}

void WsBridgeEvent::dump_config() {
  LOG_EVENT("", "WS Bridge Event", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

// No state push here, unlike every other domain: events are one-shot and HA
// restores no last state for them, so re-sending the last event_type on each
// (re)declare would replay a doorbell press that already happened.
void WsBridgeEvent::ws_bridge_declare() {
  event::Event &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_event(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
}

}  // namespace ws_bridge
}  // namespace esphome
