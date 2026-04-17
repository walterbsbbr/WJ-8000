#include "DW8000Delay.h"
#include <juce_core/juce_core.h>

void DW8000Delay::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
    buf_.assign(MAX_BUFFER_SAMPLES, 0.0f);
    reset();
}

void DW8000Delay::reset()
{
    std::fill(buf_.begin(), buf_.end(), 0.0f);
    writePos_  = 0;
    modPhase_  = 0.0;
}

void DW8000Delay::setParams(uint8_t time, uint8_t factor,
                              uint8_t feedback, uint8_t modFreq,
                              uint8_t modIntensity, uint8_t effectLevel)
{
    float ms     = computeDelayMs(time, factor);
    baseDelaySamples_ = ms * 0.001f * sampleRate_;
    baseDelaySamples_ = std::clamp(baseDelaySamples_, 1.0f, (float)(MAX_BUFFER_SAMPLES - 2));

    feedback_ = (float)feedback * (0.95f / 15.0f);  // 0 → 0, 15 → ~0.95

    // Wet level: 0 → 0, 15 → 1.0
    wetLevel_ = (float)effectLevel / 15.0f;

    // Mod LFO: rate 0–31 → 0–10 Hz
    float modHz      = (float)modFreq * (10.0f / 31.0f);
    modPhaseInc_     = modHz / sampleRate_;

    // Mod depth: 0–31 → 0 to 50% of delay time
    modDepthSamples_ = baseDelaySamples_ * 0.5f * ((float)modIntensity / 31.0f);
}

float DW8000Delay::readInterp(float delaySamples) const
{
    // Read from circular buffer with linear interpolation
    float readPos = (float)writePos_ - delaySamples;
    int   bufSz   = (int)buf_.size();

    while (readPos < 0.0f) readPos += (float)bufSz;

    int   i0   = (int)readPos % bufSz;
    int   i1   = (i0 + 1)    % bufSz;
    float frac = readPos - (float)(int)readPos;
    return buf_[i0] + frac * (buf_[i1] - buf_[i0]);
}

float DW8000Delay::processSample(float input)
{
    if (wetLevel_ < 0.0001f)
        return input;

    // Modulated delay time (chorus/flange LFO)
    float modLFO = std::sin((float)(modPhase_ * 2.0 * juce::MathConstants<double>::pi));
    modPhase_ += modPhaseInc_;
    if (modPhase_ >= 1.0) modPhase_ -= 1.0;

    float delaySamples = baseDelaySamples_ + modLFO * modDepthSamples_;
    delaySamples = std::clamp(delaySamples, 1.0f, (float)(MAX_BUFFER_SAMPLES - 2));

    float delayed = readInterp(delaySamples);
    float feedback = delayed * feedback_;

    // Write to buffer
    buf_[writePos_] = input + feedback;
    writePos_ = (writePos_ + 1) % (int)buf_.size();

    return input + delayed * wetLevel_;
}
