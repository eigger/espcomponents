#pragma once
#include <vector>
#include <queue>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/i2s_audio/microphone/i2s_audio_microphone.h"
#include <arduinoFFT.h>
#include "version.h"
namespace esphome {
namespace note_finder {

class NoteFinderComponent : public i2s_audio::I2SAudioMicrophone, public number::Number
{
public:
    NoteFinderComponent() = default;
    void dump_config() override;
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

    unsigned long elapsed_time(const unsigned long timer);
    unsigned long get_time();
    void control(float value) override;
    void set_version(text_sensor::TextSensor *version) { version_ = version; }
    void set_note(text_sensor::TextSensor *note) { note_ = note; }
    void set_frequency(sensor::Sensor *frequency) { frequency_ = frequency; }
    void set_octave(sensor::Sensor *octave) { octave_ = octave; }
    void set_amplitude(sensor::Sensor *amplitude) { amplitude_ = amplitude; }

protected:
    void callback_(const std::vector<int16_t> &data);
    text_sensor::TextSensor *version_{nullptr};
    text_sensor::TextSensor *note_{nullptr};
    sensor::Sensor *frequency_{nullptr};
    sensor::Sensor *octave_{nullptr};
    sensor::Sensor *amplitude_{nullptr};
    
};

} // namespace note_finder
} // namespace esphome