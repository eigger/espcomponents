#include "esphome.h"
#include <arduinoFFT.h>
#include <vector>
#include <cmath>
#include <string>

void ShowDoubleLog(std::string name, const std::vector<double> &data)
{
    std::string res;
    char temp[20]; 
    for (const auto& byte : data)
    {
        memset(temp, 0, sizeof(temp));
        sprintf(temp, "%.1lf ", byte);
        res.append(temp);
    }
    ESP_LOGD(name.c_str(), res.c_str());
}

double GetMaxFrequency(const std::vector<int16_t> &data, int wavefrom_freq = 1000)
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

    return FFT.MajorPeak();
}

std::string getNote(double frequency)
{
    static const double A4 = 440.0;
    static const double C0 = A4 * std::pow(2, -4.75);
    static const std::array<std::string, 12> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    if (frequency < C0 || frequency > C0 * std::pow(2, 9)) return "Unknown";

    double h = round(12.0 * log2(frequency / C0));
    int octave = static_cast<int>(h) / 12;
    int n = static_cast<int>(h) % 12;

    return noteNames[n] + std::to_string(octave);
}

