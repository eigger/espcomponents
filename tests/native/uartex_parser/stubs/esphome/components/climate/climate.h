#pragma once
#include "esphome/core/stub.h"
namespace esphome {
struct StringRef {
  std::string value;
  std::string str() const { return value; }
  const char* c_str() const { return value.c_str(); }
};
inline bool operator!=(const std::string& a, const StringRef& b) {
  return a != b.value;
}
namespace climate {
enum ClimateMode {
  CLIMATE_MODE_AUTO,
  CLIMATE_MODE_COOL,
  CLIMATE_MODE_DRY,
  CLIMATE_MODE_FAN_ONLY,
  CLIMATE_MODE_HEAT,
  CLIMATE_MODE_OFF
};
enum ClimateAction {
  CLIMATE_ACTION_COOLING,
  CLIMATE_ACTION_DRYING,
  CLIMATE_ACTION_FAN,
  CLIMATE_ACTION_HEATING,
  CLIMATE_ACTION_IDLE,
  CLIMATE_ACTION_OFF
};
enum ClimateSwingMode {
  CLIMATE_SWING_BOTH,
  CLIMATE_SWING_HORIZONTAL,
  CLIMATE_SWING_OFF,
  CLIMATE_SWING_VERTICAL
};
enum ClimateFanMode {
  CLIMATE_FAN_AUTO,
  CLIMATE_FAN_DIFFUSE,
  CLIMATE_FAN_FOCUS,
  CLIMATE_FAN_HIGH,
  CLIMATE_FAN_LOW,
  CLIMATE_FAN_MEDIUM,
  CLIMATE_FAN_MIDDLE,
  CLIMATE_FAN_OFF,
  CLIMATE_FAN_ON,
  CLIMATE_FAN_QUIET
};
enum ClimatePreset {
  CLIMATE_PRESET_ACTIVITY,
  CLIMATE_PRESET_AWAY,
  CLIMATE_PRESET_BOOST,
  CLIMATE_PRESET_COMFORT,
  CLIMATE_PRESET_ECO,
  CLIMATE_PRESET_HOME,
  CLIMATE_PRESET_NONE,
  CLIMATE_PRESET_SLEEP
};
constexpr int CLIMATE_SUPPORTS_ACTION = 0;
constexpr int CLIMATE_SUPPORTS_CURRENT_HUMIDITY = 0;
constexpr int CLIMATE_SUPPORTS_CURRENT_TEMPERATURE = 0;
constexpr int CLIMATE_SUPPORTS_TARGET_HUMIDITY = 0;
constexpr int CLIMATE_SUPPORTS_TWO_POINT_TARGET_TEMPERATURE = 0;
struct ClimateTraits {
  template <typename T>
  void add_feature_flags(T) {}
  template <typename T>
  void clear_feature_flags(T) {}
  template <typename T>
  void add_supported_mode(T) {}
  template <typename T>
  void add_supported_swing_mode(T) {}
  template <typename T>
  void add_supported_fan_mode(T) {}
  template <typename T>
  void add_supported_preset(T) {}
};
struct ClimateCall {
  optional<ClimateMode> mode;
  optional<float> target_temperature, target_humidity;
  optional<ClimateSwingMode> swing_mode;
  optional<ClimateFanMode> fan_mode;
  optional<ClimatePreset> preset;
  auto get_mode() const { return mode; }
  auto get_target_temperature() const { return target_temperature; }
  auto get_target_humidity() const { return target_humidity; }
  auto get_swing_mode() const { return swing_mode; }
  auto get_fan_mode() const { return fan_mode; }
  auto get_preset() const { return preset; }
  bool has_custom_fan_mode() const { return false; }
  bool has_custom_preset() const { return false; }
  StringRef get_custom_fan_mode() const { return {}; }
  StringRef get_custom_preset() const { return {}; }
};
class Climate : public EntityBase {
 public:
  ClimateMode mode = CLIMATE_MODE_OFF;
  ClimateAction action = CLIMATE_ACTION_OFF;
  ClimateSwingMode swing_mode = CLIMATE_SWING_OFF;
  optional<ClimateFanMode> fan_mode;
  optional<ClimatePreset> preset;
  float target_temperature = NAN, current_temperature = NAN,
        target_humidity = NAN, current_humidity = NAN;
  void publish_state() {}
  std::string get_custom_fan_mode() const { return {}; }
  std::string get_custom_preset() const { return {}; }
  bool set_custom_fan_mode_(const char*) { return true; }
  bool set_custom_preset_(const char*) { return true; }
  void set_supported_custom_fan_modes(const std::vector<const char*>&) {}
  void set_supported_custom_presets(const std::vector<const char*>&) {}

 protected:
  virtual ~Climate() = default;
  virtual void control(const ClimateCall&) {}
  virtual ClimateTraits traits() { return {}; }
};
}  // namespace climate
}  // namespace esphome
