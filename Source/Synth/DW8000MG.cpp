#include "DW8000MG.h"

void DW8000MG::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
}

void DW8000MG::reset()
{
    phase_        = 0.0;
    delayCounter_ = 0.0f;
    delayGain_    = 0.0f;
}

void DW8000MG::setParams(uint8_t waveform, uint8_t freq, uint8_t delay,
                          uint8_t oscDepth, uint8_t vcfDepth)
{
    waveform_    = waveform & 0x03;
    freqHz_      = freqParamToHz(freq);
    delayTime_   = delayParamToSec(delay);
    delaySamples_= delayTime_ * sampleRate_;

    // Depth 0–31 → max semitones: ~±2 semitones for vibrato feels right
    // (hardware is subtle: 31 ≈ about 1-2 semitones of vibrato)
    oscDepth_    = (float)oscDepth * (2.0f / 31.0f);
    vcfDepth_    = (float)vcfDepth * (6.0f / 31.0f);  // filter: up to 6 semitones
}

void DW8000MG::processSample(float& oscSemitones, float& vcfSemitones)
{
    // ── Delay fade-in ────────────────────────────────────────────────
    if (delayCounter_ < delaySamples_)
    {
        delayCounter_ += 1.0f;
        delayGain_ = delayCounter_ / (delaySamples_ + 1.0f);
    }
    else
    {
        delayGain_ = 1.0f;
    }

    // ── LFO waveform ─────────────────────────────────────────────────
    float lfo = 0.0f;
    switch (waveform_)
    {
        case 0:  // Triangle
            lfo = (float)(phase_ < 0.5 ? 4.0 * phase_ - 1.0 : 3.0 - 4.0 * phase_);
            break;
        case 1:  // Up sawtooth
            lfo = (float)(2.0 * phase_ - 1.0);
            break;
        case 2:  // Down sawtooth
            lfo = (float)(1.0 - 2.0 * phase_);
            break;
        case 3:  // Square
            lfo = (phase_ < 0.5) ? 1.0f : -1.0f;
            break;
    }

    lfo *= delayGain_;

    // ── Phase advance ────────────────────────────────────────────────
    phase_ += freqHz_ / sampleRate_;
    if (phase_ >= 1.0) phase_ -= 1.0;

    oscSemitones = lfo * oscDepth_;
    vcfSemitones = lfo * vcfDepth_;
}
