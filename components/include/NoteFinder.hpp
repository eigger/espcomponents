#include "esphome.h"
#include <arduinoFFT.h>
#include <vector>
#include <cmath>
#include <string>

class NoteFinder
{
public:
    double Frequency;
    double Amplitude;
    std::string Note;
    int Octave;
    int SamplingFrequency;
    NoteFinder(int samplingFrequency = 16000)
    {
        Frequency = 0;
        Amplitude = 0;
        SamplingFrequency = samplingFrequency;
    }

    bool FindMajorPeak(const std::vector<int16_t> &data, double minAmplitude)
    {
        size_t data_size = data.size();
        std::vector<double> vReal(data_size, 0.0);
        std::vector<double> vImag(data_size, 0.0);
        for (size_t i = 0; i < data_size; i++)
        {
            vReal[i] = static_cast<double>(data[i]); 
        }
        arduinoFFT FFT(vReal.data(), vImag.data(), data_size, SamplingFrequency);
        FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
        FFT.Compute(FFT_FORWARD);
        FFT.ComplexToMagnitude();
        FFT.MajorPeak(&Frequency, &Amplitude);
        if (Amplitude < minAmplitude) return false;
        return true;
    }

    bool FindNote(double frequency)
    {
        double A4 = 440.0;
        double C0 = A4 * std::pow(2, -4.75);
        //std::array<std::string, 12> noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        std::array<std::string, 12> noteNames = {"도", "도#", "레", "레#", "미", "파", "파#", "솔", "솔#", "라", "라#", "시"};
        if (frequency >= C0 && frequency <= C0 * std::pow(2, 9))
        {
            double h = round(12.0 * log2(frequency / C0));
            int octave = static_cast<int>(h) / 12;
            int n = static_cast<int>(h) % 12;
            Octave = octave;
            Note = noteNames[n];
            return true;
        }

        Octave = 0;
        Note = "Unknown";
        return false;
    }
};
