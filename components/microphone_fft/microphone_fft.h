#pragma once
#include <vector>
#include <queue>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2s_audio/microphone/i2s_audio_microphone.h"
#include <arduinoFFT.h>
#include "version.h"
namespace esphome {
namespace microphone_fft {

class MicrophoneFFT : public i2s_audio::I2SAudioMicrophone
{
public:
    MicrophoneFFT() = default;
    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_max_frequency(sensor::Sensor *max_frequency) { max_frequency_ = max_frequency; }
    //void set_people_count(number::Number *people_count) { people_count_ = people_count; }
protected:
    void callback_(const std::vector<uint16_t> &data);
    text_sensor::TextSensor *version_{nullptr};
    sensor::Sensor *max_frequency_{nullptr};
};

} // namespace microphone_fft
} // namespace esphome