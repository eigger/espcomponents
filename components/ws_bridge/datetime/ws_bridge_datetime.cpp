#include "ws_bridge_datetime.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.datetime";

// The three control() overrides below are only reached when NOT wrapping — see
// WsBridgeSwitch::write_state(). They write the call's fields straight through
// and publish, like `platform: template` in optimistic mode.

#ifdef USE_DATETIME_DATE
void WsBridgeDate::setup() { ws_subscribe_date(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeDate::dump_config() {
  LOG_DATETIME_DATE("", "WS Bridge Date", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

void WsBridgeDate::control(const datetime::DateCall &call) {
  if (call.get_year().has_value())
    this->year_ = *call.get_year();
  if (call.get_month().has_value())
    this->month_ = *call.get_month();
  if (call.get_day().has_value())
    this->day_ = *call.get_day();
  this->publish_state();
}

void WsBridgeDate::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_date(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeDate::ws_bridge_declare() {
  datetime::DateEntity &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_date(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_date(this, src);
}
#endif  // USE_DATETIME_DATE

#ifdef USE_DATETIME_TIME
void WsBridgeTime::setup() { ws_subscribe_time(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeTime::dump_config() {
  LOG_DATETIME_TIME("", "WS Bridge Time", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

void WsBridgeTime::control(const datetime::TimeCall &call) {
  if (call.get_hour().has_value())
    this->hour_ = *call.get_hour();
  if (call.get_minute().has_value())
    this->minute_ = *call.get_minute();
  if (call.get_second().has_value())
    this->second_ = *call.get_second();
  this->publish_state();
}

void WsBridgeTime::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_time(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeTime::ws_bridge_declare() {
  datetime::TimeEntity &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_time(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_time(this, src);
}
#endif  // USE_DATETIME_TIME

#ifdef USE_DATETIME_DATETIME
void WsBridgeDateTime::setup() { ws_subscribe_datetime(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeDateTime::dump_config() {
  LOG_DATETIME_DATETIME("", "WS Bridge DateTime", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

void WsBridgeDateTime::control(const datetime::DateTimeCall &call) {
  if (call.get_year().has_value())
    this->year_ = *call.get_year();
  if (call.get_month().has_value())
    this->month_ = *call.get_month();
  if (call.get_day().has_value())
    this->day_ = *call.get_day();
  if (call.get_hour().has_value())
    this->hour_ = *call.get_hour();
  if (call.get_minute().has_value())
    this->minute_ = *call.get_minute();
  if (call.get_second().has_value())
    this->second_ = *call.get_second();
  this->publish_state();
}

void WsBridgeDateTime::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_datetime(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeDateTime::ws_bridge_declare() {
  datetime::DateTimeEntity &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_datetime(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_datetime(this, src);
}
#endif  // USE_DATETIME_DATETIME

}  // namespace ws_bridge
}  // namespace esphome
