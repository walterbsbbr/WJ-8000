#include "NJM2069.h"
#include <juce_core/juce_core.h>

static const float kbdTrackTable[4] = { 0.0f, 0.25f, 0.5f, 1.0f };

void NJM2069::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void NJM2069::reset()
{
    for (auto& s : s_) s = 0.0f;
    f_  = 0.0f;
    fb_ = 0.0f;
}

void NJM2069::updateParams(uint8_t cutoffParam, uint8_t resonance,
                            uint8_t kbdTrackParam, int midiNote,
                            float egAmount, float mgAmount, float aftertouchAmt,
                            float pitchBendVcfAmt)
{
    // ── Cutoff ───────────────────────────────────────────────────────────
    float cutHz = paramToHz((float)cutoffParam);

    // Keyboard tracking: semitone offset from C4 (MIDI 60), scaled
    float track = kbdTrackTable[kbdTrackParam & 0x03];
    float semitoneOffset = (midiNote - 60) * track;
    cutHz *= std::pow(2.0f, semitoneOffset / 12.0f);

    // EG modulation (signed, already scaled by EG intensity)
    // egAmount is in semitones (-31..+31 range roughly)
    cutHz *= std::pow(2.0f, egAmount / 12.0f);

    // MG and aftertouch (add semitones)
    cutHz *= std::pow(2.0f, mgAmount / 12.0f);
    cutHz *= std::pow(2.0f, aftertouchAmt / 12.0f);

    // Pitch bend → VCF (optional, on/off)
    cutHz *= std::pow(2.0f, pitchBendVcfAmt / 12.0f);

    cutHz = std::clamp(cutHz, 20.0f, sampleRate_ * 0.49f);

    // Bilinear prewarped coefficient
    float w = std::tan(juce::MathConstants<float>::pi * cutHz / sampleRate_);
    f_ = w / (1.0f + w);

    // ── Resonance ────────────────────────────────────────────────────────
    // resonance 0–31 → feedback 0..~4.0  (self-osc at 31 → ≈3.8)
    fb_ = (float)resonance * (3.8f / 31.0f);
}

// Huovilainen improved Moog 4-pole ladder with nonlinear feedback
float NJM2069::processSample(float input)
{
    // Feedback from last stage
    float feedback = fb_ * s_[3];

    float x = input - feedback;
    x = tanhApprox(x);

    // 4 cascaded one-pole lowpass stages with tanh saturation
    float g   = f_;
    float s0  = s_[0] + g * (tanhApprox(x)   - tanhApprox(s_[0]));
    float s1  = s_[1] + g * (tanhApprox(s0)  - tanhApprox(s_[1]));
    float s2  = s_[2] + g * (tanhApprox(s1)  - tanhApprox(s_[2]));
    float s3  = s_[3] + g * (tanhApprox(s2)  - tanhApprox(s_[3]));

    s_[0] = s0; s_[1] = s1; s_[2] = s2; s_[3] = s3;

    return s3;
}
