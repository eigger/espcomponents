#include "note_finder.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace note_finder {
static const char *TAG = "note_finder";
void NoteFinderComponent::dump_config()
{
    
}

void NoteFinderComponent::setup()
{
    if (this->version_) this->version_->publish_state(VERSION);
    add_data_callback(std::bind(&NoteFinderComponent::callback_, this, std::placeholders::_1));
    ESP_LOGI(TAG, "Initaialize.");
}


void NoteFinderComponent::callback_(const std::vector<uint16_t> &data)
{
    size_t data_size = data.size();
    std::vector<double> vReal(data_size, 0.0);
    std::vector<double> vImag(data_size, 0.0);

    for (size_t i = 0; i < data_size; i++)
    {
        vReal[i] = static_cast<double>(data[i]); 
    }

    arduinoFFT FFT(vReal.data(), vImag.data(), data_size, wavefrom_freq);
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
    
    if (this->frequency_) this->frequency_->publish_state(FFT.MajorPeak());
}

void NoteFinderComponent::control(float value)
{
    if (this->state != value)
    {
        this->state = value;
    }
}

unsigned long NoteFinderComponent::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long NoteFinderComponent::get_time()
{
    return millis();
}

}  // namespace note_finder
}  // namespace esphome