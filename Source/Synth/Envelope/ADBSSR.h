#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

// 6-stage ADBSSR envelope: Attack → Decay → Breakpoint → Slope → Sustain → Release
// Matches the Korg DW-8000 hardware envelope behaviour.
class ADBSSR
{
public:
    enum class Stage { Idle, Attack, Decay, Slope, Sustain, Release };

    struct Params
    {
        uint8_t attack     = 5;   // 0–31
        uint8_t decay      = 15;  // 0–31  (target = breakpoint level)
        uint8_t breakPoint = 20;  // 0–31  (level after decay)
        uint8_t slopeTime  = 10;  // 0–31  (time from breakpoint to sustain)
        uint8_t sustain    = 15;  // 0–31  (level during sustain)
        uint8_t release    = 10;  // 0–31
        uint8_t velSens    = 3;   // 0–7
    };

    void prepare(float sampleRate);
    void setParams(const Params& p);

    void noteOn(float velocity);   // velocity 0.0–1.0
    void noteOff();
    void reset();

    float processSample();

    Stage stage()  const { return stage_; }
    bool  isIdle() const { return stage_ == Stage::Idle; }

private:
    float sampleRate_ = 48000.0f;
    Stage stage_      = Stage::Idle;
    float output_     = 0.0f;
    float peakLevel_  = 1.0f;   // velocity-scaled

    // Pre-computed per-sample coefficients (exponential approach)
    float attackCoeff_  = 0.0f;
    float decayCoeff_   = 0.0f;
    float slopeCoeff_   = 0.0f;
    float releaseCoeff_ = 0.0f;

    // Normalised levels (0.0–1.0)
    float breakNorm_   = 0.0f;
    float sustainNorm_ = 0.0f;

public:
    // Hardware-accurate time table: param 0–31 → seconds
    // Approximated from service manual measurements
    static constexpr float kTimeTable[32] = {
        0.001f, 0.002f, 0.003f, 0.005f, 0.008f, 0.012f, 0.018f, 0.025f,
        0.035f, 0.050f, 0.070f, 0.100f, 0.140f, 0.200f, 0.280f, 0.400f,
        0.550f, 0.770f, 1.100f, 1.500f, 2.100f, 3.000f, 4.200f, 6.000f,
        8.500f,12.000f,17.000f,24.000f,30.000f,30.000f,30.000f,30.000f
    };

    // Convert param time to one-pole lowpass coefficient
    static float timeToCoeff(uint8_t param, float sr)
    {
        float t = kTimeTable[param & 0x1F];
        // coefficient for exponential approach: c = exp(-1 / (t * sr))
        return std::exp(-1.0f / (t * sr));
    }
};
