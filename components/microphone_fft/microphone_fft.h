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

class MicrophoneFFT : public i2s_audio::I2SAudioMicrophone, public Component
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
    void set_125(sensor::Sensor *band) { band125_ = band; }
    void set_250(sensor::Sensor *band) { band250_ = band; }
    void set_500(sensor::Sensor *band) { band500_ = band; }
    void set_1k(sensor::Sensor *band) { band1k_ = band; }
    void set_2k(sensor::Sensor *band) { band2k_ = band; }
    void set_4k(sensor::Sensor *band) { band4k_ = band; }
    void set_8k(sensor::Sensor *band) { band8k_ = band; }
    void set_16k(sensor::Sensor *band) { band16k_ = band; }
    //void set_people_count(number::Number *people_count) { people_count_ = people_count; }
protected:
    void callback_(const std::vector<uint8_t> &data);
    text_sensor::TextSensor *version_{nullptr};
    sensor::Sensor *band125_{nullptr};
    sensor::Sensor *band250_{nullptr};
    sensor::Sensor *band500_{nullptr};
    sensor::Sensor *band1k_{nullptr};
    sensor::Sensor *band2k_{nullptr};
    sensor::Sensor *band4k_{nullptr};
    sensor::Sensor *band8k_{nullptr};
    sensor::Sensor *band16k_{nullptr};
    arduinoFFT FFT;
};

} // namespace microphone_fft
} // namespace esphome