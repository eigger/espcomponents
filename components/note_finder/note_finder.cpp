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


void NoteFinderComponent::callback_(const std::vector<int16_t> &data)
{
    int sampling_freq = 16000;
    double frequency = 0.0;
    double amplitude = 0.0;
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
    FFT.MajorPeak(&frequency, &amplitude);
    //if (this->state > amplitude) return;
    double A4 = 440.0;
    double C0 = A4 * std::pow(2, -4.75);
    //std::array<std::string, 12> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    std::array<std::string, 12> noteNames = {"도", "도#", "레", "레#", "미", "파", "파#", "솔", "솔#", "라", "라#", "시"};
    if (frequency < C0 || frequency > C0 * std::pow(2, 9)) return;
    
    double h = round(12.0 * log2(frequency / C0));
    int octave = static_cast<int>(h) / 12;
    int n = static_cast<int>(h) % 12;

    // Note = noteNames[n];
    // Octave = octave;
    
    if (this->frequency_) this->frequency_->publish_state(frequency);
    if (this->amplitude_) this->amplitude_->publish_state(amplitude);
    if (this->octave_) this->octave_->publish_state(octave);
    if (this->note_) this->note_->publish_state(noteNames[n]);
}

// void NoteFinderComponent::control(float value)
// {
//     if (this->state != value)
//     {
//         this->state = value;
//     }
// }

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