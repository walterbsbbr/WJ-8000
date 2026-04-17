#pragma once
#include <cstdint>
#include <cmath>

// MG = Modulation Generator (LFO) for the DW-8000.
// Global (one per engine), applied per voice in the engine render loop.
class DW8000MG
{
public:
    void prepare(float sampleRate);
    void reset();

    // Raw DW-8000 params
    void setParams(uint8_t waveform, uint8_t freq, uint8_t delay,
                   uint8_t oscDepth, uint8_t vcfDepth);

    // Returns modulation for this sample.
    // oscSemitones and vcfSemitones are outputs.
    void processSample(float& oscSemitones, float& vcfSemitones);

private:
    float sampleRate_ = 48000.0f;

    uint8_t waveform_ = 0;   // 0=Tri, 1=UpSaw, 2=DnSaw, 3=Sq
    float   freqHz_   = 0.5f;
    float   delayTime_= 0.0f;  // seconds
    float   oscDepth_ = 0.0f;  // semitones peak
    float   vcfDepth_ = 0.0f;  // semitones peak

    double phase_       = 0.0;
    float  delaySamples_= 0.0f;
    float  delayCounter_= 0.0f;
    float  delayGain_   = 0.0f;  // 0→1 fade-in

    // Param 0–31 → Hz: ~0.1 to ~10 Hz
    static float freqParamToHz(uint8_t p)
    {
        return 0.1f * std::pow(100.0f, (float)p / 31.0f);
    }

    // Param 0–31 → seconds delay: ~0 to ~5s
    static float delayParamToSec(uint8_t p)
    {
        if (p == 0) return 0.0f;
        return 0.05f * std::pow(100.0f, (float)(p - 1) / 30.0f);
    }
};
