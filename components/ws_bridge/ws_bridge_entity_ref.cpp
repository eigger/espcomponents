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

#ifdef USE_LIGHT
void WsBridgeLightRef::setup() { ws_subscribe_light(this->source_, this); }
void WsBridgeLightRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> light '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeLightRef::ws_bridge_declare() {
  ws_declare_light(this, *this->source_,
                   ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_light(this, *this->source_);
}
void WsBridgeLightRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_light(this->source_, command);
}
void WsBridgeLightRef::on_light_remote_values_update() { ws_send_state_light(this, *this->source_); }
#endif

#ifdef USE_COVER
void WsBridgeCoverRef::setup() { ws_subscribe_cover(this, this->source_); }
void WsBridgeCoverRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> cover '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeCoverRef::ws_bridge_declare() {
  ws_declare_cover(this, *this->source_, nullptr,
                   ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_cover(this, *this->source_);
}
void WsBridgeCoverRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_cover(this->source_, command);
}
#endif

#ifdef USE_CLIMATE
void WsBridgeClimateRef::setup() { ws_subscribe_climate(this, this->source_); }
void WsBridgeClimateRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> climate '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeClimateRef::ws_bridge_declare() {
  ws_declare_climate(this, *this->source_, nullptr,
                     ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_climate(this, *this->source_);
}
void WsBridgeClimateRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_climate(this->source_, command);
}
#endif

#ifdef USE_FAN
void WsBridgeFanRef::setup() { ws_subscribe_fan(this, this->source_); }
void WsBridgeFanRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> fan '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeFanRef::ws_bridge_declare() {
  ws_declare_fan(this, *this->source_, nullptr,
                 ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_fan(this, *this->source_);
}
void WsBridgeFanRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_fan(this->source_, command);
}
#endif

#ifdef USE_TEXT
void WsBridgeTextRef::setup() { ws_subscribe_text(this, this->source_); }
void WsBridgeTextRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> text '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeTextRef::ws_bridge_declare() {
  ws_declare_text(this, *this->source_, nullptr,
                  ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_text(this, *this->source_);
}
void WsBridgeTextRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_text(this->source_, command);
}
#endif

#ifdef USE_LOCK
void WsBridgeLockRef::setup() { ws_subscribe_lock(this, this->source_); }
void WsBridgeLockRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> lock '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeLockRef::ws_bridge_declare() {
  // entities: has no per-field overrides at all, and code_format has no ESPHome
  // counterpart to inherit from — use `platform: ws_bridge` with `lock_id:` to
  // set one.
  const std::string no_code_format;
  ws_declare_lock(this, *this->source_, nullptr,
                  ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()), no_code_format);
  ws_push_state_lock(this, *this->source_);
}
void WsBridgeLockRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_lock(this->source_, command);
}
#endif

#ifdef USE_VALVE
void WsBridgeValveRef::setup() { ws_subscribe_valve(this, this->source_); }
void WsBridgeValveRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> valve '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeValveRef::ws_bridge_declare() {
  ws_declare_valve(this, *this->source_, nullptr,
                   ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_valve(this, *this->source_);
}
void WsBridgeValveRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_valve(this->source_, command);
}
#endif

#ifdef USE_EVENT
void WsBridgeEventRef::setup() { ws_subscribe_event(this, this->source_); }
void WsBridgeEventRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> event '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
// No state push on declare — see ws_subscribe_event()'s comment.
void WsBridgeEventRef::ws_bridge_declare() {
  ws_declare_event(this, *this->source_, nullptr,
                   ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
}
#endif

#ifdef USE_DATETIME_DATE
void WsBridgeDateRef::setup() { ws_subscribe_date(this, this->source_); }
void WsBridgeDateRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> date '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeDateRef::ws_bridge_declare() {
  ws_declare_date(this, *this->source_, nullptr,
                  ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_date(this, *this->source_);
}
void WsBridgeDateRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_date(this->source_, command);
}
#endif

#ifdef USE_DATETIME_TIME
void WsBridgeTimeRef::setup() { ws_subscribe_time(this, this->source_); }
void WsBridgeTimeRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> time '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeTimeRef::ws_bridge_declare() {
  ws_declare_time(this, *this->source_, nullptr,
                  ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_time(this, *this->source_);
}
void WsBridgeTimeRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_time(this->source_, command);
}
#endif

#ifdef USE_DATETIME_DATETIME
void WsBridgeDateTimeRef::setup() { ws_subscribe_datetime(this, this->source_); }
void WsBridgeDateTimeRef::dump_config() {
  ESP_LOGCONFIG(TAG, "WS Bridge Entity Ref '%s' -> datetime '%s'", this->get_ws_bridge_unique_id().c_str(),
                this->source_->get_name().str().c_str());
}
void WsBridgeDateTimeRef::ws_bridge_declare() {
  ws_declare_datetime(this, *this->source_, nullptr,
                      ws_ha_name(*this->source_, this->name_override_, this->get_ws_bridge_unique_id()));
  ws_push_state_datetime(this, *this->source_);
}
void WsBridgeDateTimeRef::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_datetime(this->source_, command);
}
#endif

}  // namespace ws_bridge
}  // namespace esphome
