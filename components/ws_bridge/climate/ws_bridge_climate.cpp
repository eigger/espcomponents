#include "ws_bridge_climate.h"
#include "../ws_bridge_domains.h"

namespace esphome {
namespace ws_bridge {

static const char *const TAG = "ws_bridge.climate";

void WsBridgeClimate::setup() { ws_subscribe_climate(this, this->source_ != nullptr ? this->source_ : this); }

void WsBridgeClimate::dump_config() {
  LOG_CLIMATE("", "WS Bridge Climate", this);
  if (this->source_ != nullptr)
    ESP_LOGCONFIG(TAG, "  Wrapped: '%s'", this->source_->get_name().str().c_str());
}

climate::ClimateTraits WsBridgeClimate::traits() {
  if (this->source_ != nullptr)
    return this->source_->get_traits();
  climate::ClimateTraits traits;
  // No CURRENT_TEMPERATURE — standalone never invents a sensor reading.
  // ACTION is kept and derived from mode in control().
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT_COOL,
      climate::CLIMATE_MODE_FAN_ONLY,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_AUTO,
  });
  return traits;
}

static climate::ClimateAction ws_bridge_action_for_mode_(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_OFF:
      return climate::CLIMATE_ACTION_OFF;
    case climate::CLIMATE_MODE_HEAT:
      return climate::CLIMATE_ACTION_HEATING;
    case climate::CLIMATE_MODE_COOL:
      return climate::CLIMATE_ACTION_COOLING;
    case climate::CLIMATE_MODE_DRY:
      return climate::CLIMATE_ACTION_DRYING;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return climate::CLIMATE_ACTION_FAN;
    case climate::CLIMATE_MODE_HEAT_COOL:
    case climate::CLIMATE_MODE_AUTO:
    default:
      return climate::CLIMATE_ACTION_IDLE;
  }
}

// Only reached when NOT wrapping — optimistic like cover/valve.
void WsBridgeClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    this->action = ws_bridge_action_for_mode_(this->mode);
  }
  if (call.get_target_temperature().has_value())
    this->target_temperature = *call.get_target_temperature();
  if (call.get_target_temperature_low().has_value())
    this->target_temperature_low = *call.get_target_temperature_low();
  if (call.get_target_temperature_high().has_value())
    this->target_temperature_high = *call.get_target_temperature_high();
  if (call.get_target_humidity().has_value())
    this->target_humidity = *call.get_target_humidity();
  if (call.get_fan_mode().has_value())
    this->set_fan_mode_(*call.get_fan_mode());
  if (call.has_custom_fan_mode())
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();
  if (call.get_preset().has_value())
    this->set_preset_(*call.get_preset());
  if (call.has_custom_preset())
    this->set_custom_preset_(call.get_custom_preset());
  this->publish_state();
}

void WsBridgeClimate::ws_bridge_handle_command(const WsCommand &command) {
  ws_handle_command_climate(this->source_ != nullptr ? this->source_ : this, command);
}

void WsBridgeClimate::ws_bridge_declare() {
  climate::Climate &src = this->source_ != nullptr ? *this->source_ : *this;
  const std::string own_name = this->has_own_name() ? this->get_name().str() : "";
  ws_declare_climate(this, src, this, ws_ha_name(src, own_name, this->unique_id_));
  ws_push_state_climate(this, src);
}

}  // namespace ws_bridge
}  // namespace esphome
