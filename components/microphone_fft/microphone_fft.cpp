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
    add_data_callback(std::bind(&MicrophoneFFT::callback_, this, std::placeholders::_1));
    ESP_LOGI(TAG, "Initaialize.");
}

void MicrophoneFFT::callback_(const std::vector<uint16_t> &data)
{
    int sampling_freq = 16000;
    size_t data_size = data.size();
    std::vector<double> vReal(data_size, 0.0);
    std::vector<double> vImag(data_size, 0.0);

    for (size_t i = 0; i < data_size; i++)
    {
        vReal[i] = static_cast<double>(data[i]); 
    }

    arduinoFFT FFT(vReal.data(), vImag.data(), data_size, sampling_freq);
    FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.Compute(FFT_FORWARD);
    FFT.ComplexToMagnitude();
    FFT.MajorPeak(&Frequency, &Value);

    double A4 = 440.0;
    double C0 = A4 * std::pow(2, -4.75);
    //std::array<std::string, 12> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    std::array<std::string, 12> noteNames = {"도", "도#", "레", "레#", "미", "파", "파#", "솔", "솔#", "라", "라#", "시"};
    if (Frequency < C0 || Frequency > C0 * std::pow(2, 9))
    {
        Note = "Unknown";
        Octave = 0;
    }
    else
    {
        double h = round(12.0 * log2(Frequency / C0));
        int octave = static_cast<int>(h) / 12;
        int n = static_cast<int>(h) % 12;

        Note = noteNames[n];
        Octave = octave;
    }
    
    if (this->max_frequency_) this->max_frequency_->publish_state(FFT.MajorPeak());
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