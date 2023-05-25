#include "microphone_fft.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace microphone_fft {
static const char *TAG = "microphone_fft";
void MicrophoneFFT::dump_config()
{
}

void MicrophoneFFT::setup()
{
    if (this->version_) this->version_->publish_state(VERSION);
    FFT = arduinoFFT();
    add_data_callback(callback_);
    ESP_LOGI(TAG, "Initaialize.");
}

void MicrophoneFFT::callback_(const std::vector<uint8_t> &data)
{
    const uint8_t amplitude = 150;
    std::vector<double> vReal;
    std::vector<double> vImag;
    int bands[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    for (uint16_t i = 0; i < data.size(); i++) {
        vReal.push_back(data[i] << 8);
        vImag.push_back(0.0); //Imaginary part must be zeroed in case of looping to avoid wrong calculations and overflows
    }

    FFT.Windowing(&vReal[0], data.size(), FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(&vReal[0], &vImag[0], data.size(), FFT_FORWARD);
    FFT.ComplexToMagnitude(&vReal[0], &vImag[0], data.size());
    for (int i = 0; i < 8; i++) {
        bands[i] = 0;
    }
  
    for (int i = 2; i < (data.size()/2); i++){ // Don't use sample 0 and only first SAMPLES/2 are usable. Each array eleement represents a frequency and its value the amplitude.
        if (vReal[i] > 2000) { // Add a crude noise filter, 10 x amplitude or more
        if (i<=2 )             bands[0] = max(bands[0], (int)(vReal[i]/amplitude)); // 125Hz
        if (i >3   && i<=5 )   bands[1] = max(bands[1], (int)(vReal[i]/amplitude)); // 250Hz
        if (i >5   && i<=7 )   bands[2] = max(bands[2], (int)(vReal[i]/amplitude)); // 500Hz
        if (i >7   && i<=15 )  bands[3] = max(bands[3], (int)(vReal[i]/amplitude)); // 1000Hz
        if (i >15  && i<=30 )  bands[4] = max(bands[4], (int)(vReal[i]/amplitude)); // 2000Hz
        if (i >30  && i<=53 )  bands[5] = max(bands[5], (int)(vReal[i]/amplitude)); // 4000Hz
        if (i >53  && i<=200 ) bands[6] = max(bands[6], (int)(vReal[i]/amplitude)); // 8000Hz
        if (i >200           ) bands[7] = max(bands[7], (int)(vReal[i]/amplitude)); // 16000Hz
        }
    }
    if (this->band125_) this->band125_->publish_state(bands[0]);
    if (this->band250_) this->band250_->publish_state(bands[1]);
    if (this->band500_) this->band500_->publish_state(bands[2]);
    if (this->band1k_) this->band1k_->publish_state(bands[3]);
    if (this->band2k_) this->band2k_->publish_state(bands[4]);
    if (this->band4k_) this->band4k_->publish_state(bands[5]);
    if (this->band8k_) this->band8k_->publish_state(bands[6]);
    if (this->band16k_) this->band16k_->publish_state(bands[7]);
}

void MicrophoneFFT::loop()
{
}

unsigned long MicrophoneFFT::elapsed_time(const unsigned long timer)
{
    return millis() - timer;
}

unsigned long MicrophoneFFT::get_time()
{
    return millis();
}

}  // namespace microphone_fft
}  // namespace esphome