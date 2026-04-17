#pragma once
#include <cmath>
#include <algorithm>

// NJM2069 VCF emulation — 4-pole transistor-ladder low-pass filter.
// Based on the Huovilainen improved Moog model with tanh nonlinearities.
// One instance per voice, matching the DW-8000 hardware (8 chips total).
class NJM2069
{
public:
    void prepare(float sampleRate);
    void reset();

    // All params are raw DW-8000 values:
    //   cutoffParam  : 0–63
    //   resonance    : 0–31  (self-osc at 31)
    //   kbdTrackParam: 0–3   (0=off, 1=1/4, 2=1/2, 3=full)
    //   midiNote     : 0–127 (for keyboard tracking)
    //   egAmount     : -1.0..+1.0 normalised (sign from vcfEGPolarity)
    //   mgAmount     : -1.0..+1.0 normalised (MG→VCF)
    //   aftertouchAmt: 0.0..1.0 normalised (aftertouch→VCF)
    void updateParams(uint8_t cutoffParam, uint8_t resonance,
                      uint8_t kbdTrackParam, int midiNote,
                      float egAmount, float mgAmount, float aftertouchAmt,
                      float pitchBendVcfAmt);

    float processSample(float input);

private:
    float sampleRate_ = 48000.0f;

    // Ladder state (4 stages)
    float s_[4] = {};

    // Computed filter coefficients
    float f_  = 0.0f;   // normalised cutoff (0..1)
    float fb_ = 0.0f;   // feedback (resonance)

    // Fast tanh approximation (Padé)
    static float tanhApprox(float x)
    {
        // Accurate to ~0.5% for |x| < 4
        float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }

    // Map cutoffParam 0–63 to Hz: ~30 Hz at 0, ~18 kHz at 63
    static float paramToHz(float p)
    {
        return 30.0f * std::pow(600.0f, p / 63.0f);
    }
};
