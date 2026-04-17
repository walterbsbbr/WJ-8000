#include "ADBSSR.h"

constexpr float ADBSSR::kTimeTable[32];

void ADBSSR::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
}

void ADBSSR::setParams(const Params& p)
{
    attackCoeff_  = timeToCoeff(p.attack,    sampleRate_);
    decayCoeff_   = timeToCoeff(p.decay,     sampleRate_);
    slopeCoeff_   = timeToCoeff(p.slopeTime, sampleRate_);
    releaseCoeff_ = timeToCoeff(p.release,   sampleRate_);

    breakNorm_   = p.breakPoint / 31.0f;
    sustainNorm_ = p.sustain    / 31.0f;
}

void ADBSSR::noteOn(float velocity)
{
    // Velocity sensitivity: velSens=0 → fixed full, velSens=7 → pure velocity
    // Handled by caller via the velocity parameter here (already scaled)
    peakLevel_ = velocity;
    stage_     = Stage::Attack;
}

void ADBSSR::noteOff()
{
    if (stage_ != Stage::Idle)
        stage_ = Stage::Release;
}

void ADBSSR::reset()
{
    stage_  = Stage::Idle;
    output_ = 0.0f;
}

float ADBSSR::processSample()
{
    switch (stage_)
    {
        case Stage::Idle:
            output_ = 0.0f;
            break;

        case Stage::Attack:
            // Approach peakLevel_ from below
            output_ += (1.0f - output_) * (1.0f - attackCoeff_);
            if (output_ >= 0.9999f * peakLevel_)
            {
                output_ = peakLevel_;
                stage_  = Stage::Decay;
            }
            break;

        case Stage::Decay:
            // Exponential decay toward breakpoint level
            output_ += (breakNorm_ * peakLevel_ - output_) * (1.0f - decayCoeff_);
            if (std::abs(output_ - breakNorm_ * peakLevel_) < 0.0005f)
            {
                output_ = breakNorm_ * peakLevel_;
                stage_  = Stage::Slope;
            }
            break;

        case Stage::Slope:
            // Slope from breakpoint toward sustain level
            output_ += (sustainNorm_ * peakLevel_ - output_) * (1.0f - slopeCoeff_);
            if (std::abs(output_ - sustainNorm_ * peakLevel_) < 0.0005f)
            {
                output_ = sustainNorm_ * peakLevel_;
                stage_  = Stage::Sustain;
            }
            break;

        case Stage::Sustain:
            output_ = sustainNorm_ * peakLevel_;
            break;

        case Stage::Release:
            output_ += (0.0f - output_) * (1.0f - releaseCoeff_);
            if (output_ < 0.0001f)
            {
                output_ = 0.0f;
                stage_  = Stage::Idle;
            }
            break;
    }

    return output_;
}
