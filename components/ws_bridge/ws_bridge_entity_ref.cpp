#include "ws_bridge_entity_ref.h"
#include "esphome/core/log.h"
#include "ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.entity_ref";

#ifdef USE_SENSOR
void WsBridgeSensorRef::setup() { ws_subscribe_sensor(this, this->source_); }
void WsBridgeSensorRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> sensor '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeSensorRef::ws_bridge_declare() {
  ws_declare_sensor(this, *this->source_, nullptr,
                    ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_sensor(this, *this->source_);
}
#endif

#ifdef USE_BINARY_SENSOR
void WsBridgeBinarySensorRef::setup() { ws_subscribe_binary_sensor(this, this->source_); }
void WsBridgeBinarySensorRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> binary_sensor '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeBinarySensorRef::ws_bridge_declare() {
  ws_declare_binary_sensor(this, *this->source_, nullptr,
                           ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_binary_sensor(this, *this->source_);
}
#endif

#ifdef USE_TEXT_SENSOR
void WsBridgeTextSensorRef::setup() { ws_subscribe_text_sensor(this, this->source_); }
void WsBridgeTextSensorRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> text_sensor '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeTextSensorRef::ws_bridge_declare() {
  ws_declare_text_sensor(this, *this->source_, nullptr,
                         ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_text_sensor(this, *this->source_);
}
#endif

#ifdef USE_SWITCH
void WsBridgeSwitchRef::setup() { ws_subscribe_switch(this, this->source_); }
void WsBridgeSwitchRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> switch '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeSwitchRef::ws_bridge_declare() {
  ws_declare_switch(this, *this->source_, nullptr,
                    ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_switch(this, *this->source_);
}
void WsBridgeSwitchRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_switch(this->source_, command);
}
#endif

#ifdef USE_NUMBER
void WsBridgeNumberRef::setup() { ws_subscribe_number(this, this->source_); }
void WsBridgeNumberRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> number '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeNumberRef::ws_bridge_declare() {
  ws_declare_number(this, *this->source_, nullptr,
                    ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_number(this, *this->source_);
}
void WsBridgeNumberRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_number(this->source_, command);
}
#endif

#ifdef USE_SELECT
void WsBridgeSelectRef::setup() { ws_subscribe_select(this, this->source_); }
void WsBridgeSelectRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> select '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeSelectRef::ws_bridge_declare() {
  ws_declare_select(this, *this->source_, nullptr,
                    ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_select(this, *this->source_);
}
void WsBridgeSelectRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_select(this->source_, command);
}
#endif

#ifdef USE_BUTTON
void WsBridgeButtonRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> button '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeButtonRef::ws_bridge_declare() {
  ws_declare_button(this, *this->source_, nullptr,
                    ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
}
void WsBridgeButtonRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_button(this->source_, command);
}
#endif

#ifdef USE_UPDATE
void WsBridgeUpdateRef::setup() { ws_subscribe_update(this, this->source_); }
void WsBridgeUpdateRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> update '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeUpdateRef::ws_bridge_declare() {
  ws_declare_update(this, *this->source_,
                    ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_update(this, *this->source_);
}
void WsBridgeUpdateRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_update(this->source_, command);
}
#endif

}  // namespace ws_bridge
}  // namespace esphome
