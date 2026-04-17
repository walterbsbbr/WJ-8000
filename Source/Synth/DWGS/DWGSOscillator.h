#pragma once
#include "DWGSWavetableBank.h"
#include <cmath>

// One DWGS oscillator (OSC1 or OSC2 per voice).
// Uses the band-limited wavetable bank for alias-free output.
class DWGSOscillator
{
public:
    void prepare(float sampleRate, const DWGSWavetableBank* bank);
    void reset();

    // Call whenever pitch or waveform changes
    void setWaveform(int index);        // 0–15
    void setFrequency(float hz);

    float processSample();

private:
    const DWGSWavetableBank* bank_ = nullptr;
    float sampleRate_ = 48000.0f;

    int   waveform_   = 0;
    float frequency_  = 440.0f;
    double phase_     = 0.0;           // 0.0 .. 1.0
    double phaseInc_  = 0.0;
};

// Inline for audio-thread performance
inline void DWGSOscillator::setFrequency(float hz)
{
    frequency_ = hz;
    phaseInc_  = hz / sampleRate_;
}

inline float DWGSOscillator::processSample()
{
    if (!bank_ || !bank_->loaded) return 0.0f;

    const auto& tbl = bank_->tableForFreq(waveform_, frequency_);

    // ── 4-point Hermite cubic interpolation ──────────────────────────────
    // Far superior to linear: alias images ~60 dB lower, no HF rolloff.
    // TABLE_SIZE = 4096 = 2^12, so MASK = 4095 wraps all indices safely.
    constexpr int MASK = DWGSWavetableBank::TABLE_SIZE - 1;

    double pos  = phase_ * DWGSWavetableBank::TABLE_SIZE;
    int    i1   = (int)pos;
    float  frac = (float)(pos - i1);

    float y0 = tbl[(i1 - 1) & MASK];   // one sample before
    float y1 = tbl[i1];
    float y2 = tbl[i1 + 1];            // wrap sample — always safe (TABLE_SIZE+1 array)
    float y3 = tbl[(i1 + 2) & MASK];   // two samples ahead, bitmask wraps

    // Catmull-Rom / Hermite coefficients
    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    float s  = ((c3 * frac + c2) * frac + c1) * frac + c0;

    phase_ += phaseInc_;
    if (phase_ >= 1.0) phase_ -= 1.0;

    return s;
}
