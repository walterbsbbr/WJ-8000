#pragma once
#include "DWGS/DWGSOscillator.h"
#include "Filter/NJM2069.h"
#include "Envelope/ADBSSR.h"
#include "DW8000Patch.h"
#include <cstdint>

// Modulation inputs supplied by the engine each sample block
struct VoiceModulation
{
    float mgOscSemitones = 0.0f;   // LFO → pitch (semitones, ±)
    float mgVcfSemitones = 0.0f;   // LFO → filter (semitones, ±)
    float pitchBendSemitones = 0.0f;
    float pitchBendVcfSemitones = 0.0f;
    float aftertouch     = 0.0f;   // 0.0–1.0
};

class DW8000Voice
{
public:
    // ── Voice state ────────────────────────────────────────────────────
    int   midiNote  = -1;
    float velocity  = 0.0f;
    bool  active    = false;
    bool  releasing = false;
    double age      = 0.0;         // incremented each sample, for voice stealing

    // ── Lifecycle ──────────────────────────────────────────────────────
    void prepare(float sampleRate, const DWGSWavetableBank* bank);
    void noteOn (int note, float vel, const DW8000Patch& patch);
    void noteOff();
    void reset();

    // ── Per-sample audio render ────────────────────────────────────────
    // Returns one sample; call this from the engine's render loop.
    float processSample(const DW8000Patch& patch, const VoiceModulation& mod);

    bool isFinished()     const { return !active && vcaEG_.isIdle(); }
    bool vcaEGIsIdle()    const { return vcaEG_.isIdle(); }

private:
    float sampleRate_ = 48000.0f;

    DWGSOscillator osc1_, osc2_;
    NJM2069        vcf_;
    ADBSSR         vcfEG_, vcaEG_;

    // White noise state (simple LCG)
    uint32_t noiseState_ = 12345678u;
    float    nextNoise();

    // Portamento
    float portoPitch_ = 0.0f;       // current pitch in semitones (incl. note)
    float portoTarget_= 0.0f;
    float portoCoeff_ = 1.0f;       // 1.0 = instant

    // Auto-bend state
    float autoBendOffset_ = 0.0f;   // semitones offset (decays to 0)
    float autoBendCoeff_  = 1.0f;

    // DC block for output
    float dcX_ = 0.0f, dcY_ = 0.0f;

    void applyPatchParams(const DW8000Patch& patch);

    static float velScale(float vel, uint8_t sens)
    {
        // sens 0 = no velocity (full level), 7 = full velocity sensitivity
        if (sens == 0) return 1.0f;
        float t = (float)sens / 7.0f;
        return (1.0f - t) + t * vel;
    }

    static float semiToRatio(float semi) { return std::pow(2.0f, semi / 12.0f); }

    // Octave param → semitone offset from 8'
    static float octaveToSemitones(uint8_t oct)
    {
        // 0=16'(-12), 1=8'(0), 2=4'(+12)
        return (oct == 0) ? -12.0f : (oct == 2) ? 12.0f : 0.0f;
    }

    // Interval param → semitones
    static float intervalToSemitones(uint8_t iv)
    {
        static const float t[] = { 0.0f, 3.0f, 4.0f, 5.0f, 7.0f };
        return (iv < 5) ? t[iv] : 0.0f;
    }

    // Detune param → cents
    static float detuneToCents(uint8_t d)
    {
        // 0–6 → 0–25 cents linear
        return (d < 7) ? d * (25.0f / 6.0f) : 0.0f;
    }
};
