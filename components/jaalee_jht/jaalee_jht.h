#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#ifdef USE_ESP32

namespace esphome {
namespace jaalee_jht {

class JaaleeJHT : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
public:
    void set_address(uint64_t address) { address_ = address; }

    bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override;

    void dump_config() override;
    float get_setup_priority() const override { return setup_priority::DATA; }
    void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
    void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
    void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }
    void set_signal_strength(sensor::Sensor *signal_strength) { signal_strength_ = signal_strength; }

protected:
    uint64_t address_;
    sensor::Sensor *temperature_{nullptr};
    sensor::Sensor *humidity_{nullptr};
    sensor::Sensor *battery_level_{nullptr};
    sensor::Sensor *signal_strength_{nullptr};
};

}  // namespace jaalee_jht
}  // namespace esphome

#endif
