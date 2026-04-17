#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>

// KLM-775 digital delay emulation.
// Mono delay, 2ms–512ms, with internal LFO modulation for chorus/flange.
class DW8000Delay
{
public:
    // 512ms max delay + modulation headroom @ up to 192kHz
    static constexpr int MAX_BUFFER_SAMPLES = 102400;

    void prepare(float sampleRate);
    void reset();

    void setParams(uint8_t time, uint8_t factor, uint8_t feedback,
                   uint8_t modFreq, uint8_t modIntensity, uint8_t effectLevel);

    // Process one sample: input is the dry signal, returns dry+wet mix.
    float processSample(float input);

private:
    float sampleRate_  = 48000.0f;
    std::vector<float> buf_;
    int  writePos_     = 0;

    // Computed from params
    float baseDelaySamples_ = 0.0f;
    float feedback_         = 0.0f;
    float wetLevel_         = 0.0f;
    float modDepthSamples_  = 0.0f;
    float modPhaseInc_      = 0.0f;

    double modPhase_ = 0.0;

    // Delay time formula per KLM-775 spec:
    //   TIME 0 → 2–4ms,  TIME 1 → 4–8ms, ... TIME 7 → 256–512ms
    //   base_ms = 4ms × 2^TIME
    //   FACTOR 0 → ×0.5 (shortest in range), FACTOR 15 → ×1.0 (longest)
    static float computeDelayMs(uint8_t time, uint8_t factor)
    {
        float base = 4.0f * std::pow(2.0f, (float)(time & 0x07));
        return base * (0.5f + (float)(factor & 0x0F) / 30.0f);
    }

    // Interpolated read from circular buffer
    float readInterp(float delaySamples) const;
};
