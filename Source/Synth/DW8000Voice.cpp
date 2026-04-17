#include "DW8000Voice.h"
#include <juce_core/juce_core.h>

void DW8000Voice::prepare(float sampleRate, const DWGSWavetableBank* bank)
{
    sampleRate_ = sampleRate;
    osc1_.prepare(sampleRate, bank);
    osc2_.prepare(sampleRate, bank);
    vcf_.prepare(sampleRate);
    vcfEG_.prepare(sampleRate);
    vcaEG_.prepare(sampleRate);
    reset();
}

void DW8000Voice::reset()
{
    osc1_.reset();
    osc2_.reset();
    vcf_.reset();
    vcfEG_.reset();
    vcaEG_.reset();
    dcX_ = dcY_ = 0.0f;
    autoBendOffset_ = 0.0f;
    active    = false;
    releasing = false;
    age       = 0.0;
}

void DW8000Voice::noteOn(int note, float vel, const DW8000Patch& patch)
{
    midiNote  = note;
    velocity  = vel;
    active    = true;
    releasing = false;
    age       = 0.0;

    // Portamento: start from current pitch or jump
    portoTarget_ = (float)note;
    if (patch.portamentoTime == 0)
        portoPitch_ = portoTarget_;
    // else portoPitch_ keeps its previous value (glide from last note)

    // Portamento coefficient
    if (patch.portamentoTime == 0)
        portoCoeff_ = 1.0f;
    else
    {
        // Time param 0–31 → ~1ms–10s
        static const float portoTimes[32] = {
            0.0f, 0.001f, 0.003f, 0.005f, 0.008f, 0.012f, 0.018f, 0.025f,
            0.035f, 0.05f, 0.07f, 0.1f, 0.14f, 0.2f, 0.28f, 0.4f,
            0.55f, 0.77f, 1.1f, 1.5f, 2.1f, 3.0f, 4.2f, 6.0f,
            8.5f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f, 10.0f
        };
        float t = portoTimes[patch.portamentoTime];
        portoCoeff_ = std::exp(-1.0f / (t * sampleRate_));
    }

    // Auto-bend: start offset, then release toward 0
    if (patch.autoBendSelect > 0 && patch.autoBendIntensity > 0)
    {
        float intensSemitones = patch.autoBendIntensity * (12.0f / 31.0f);
        autoBendOffset_ = (patch.autoBendMode == 0) ? intensSemitones : -intensSemitones;
        float t = ADBSSR::kTimeTable[patch.autoBendTime];
        autoBendCoeff_ = std::exp(-1.0f / (t * sampleRate_));
    }
    else
    {
        autoBendOffset_ = 0.0f;
        autoBendCoeff_  = 1.0f;
    }

    applyPatchParams(patch);

    // Scale velocity by sensitivity
    float vcfVel = velScale(vel, patch.vcfVelSens);
    float vcaVel = velScale(vel, patch.vcaVelSens);

    vcfEG_.noteOn(vcfVel);
    vcaEG_.noteOn(vcaVel);
}

void DW8000Voice::noteOff()
{
    releasing = true;
    vcfEG_.noteOff();
    vcaEG_.noteOff();
}

void DW8000Voice::applyPatchParams(const DW8000Patch& patch)
{
    osc1_.setWaveform(patch.osc1Waveform);
    osc2_.setWaveform(patch.osc2Waveform);

    ADBSSR::Params vcfP;
    vcfP.attack     = patch.vcfAttack;
    vcfP.decay      = patch.vcfDecay;
    vcfP.breakPoint = patch.vcfBreakPoint;
    vcfP.slopeTime  = patch.vcfSlopeTime;
    vcfP.sustain    = patch.vcfSustain;
    vcfP.release    = patch.vcfRelease;
    vcfP.velSens    = patch.vcfVelSens;
    vcfEG_.setParams(vcfP);

    ADBSSR::Params vcaP;
    vcaP.attack     = patch.vcaAttack;
    vcaP.decay      = patch.vcaDecay;
    vcaP.breakPoint = patch.vcaBreakPoint;
    vcaP.slopeTime  = patch.vcaSlopeTime;
    vcaP.sustain    = patch.vcaSustain;
    vcaP.release    = patch.vcaRelease;
    vcaP.velSens    = patch.vcaVelSens;
    vcaEG_.setParams(vcaP);
}

float DW8000Voice::nextNoise()
{
    noiseState_ = noiseState_ * 1664525u + 1013904223u;
    return (float)(int32_t)noiseState_ / (float)0x7FFFFFFF;
}

float DW8000Voice::processSample(const DW8000Patch& patch, const VoiceModulation& mod)
{
    age += 1.0;

    if (!active && vcaEG_.isIdle())
        return 0.0f;

    // ── Portamento ───────────────────────────────────────────────────
    portoPitch_ += (portoTarget_ - portoPitch_) * (1.0f - portoCoeff_);

    // ── Auto bend (decays to 0) ───────────────────────────────────────
    autoBendOffset_ *= autoBendCoeff_;

    // ── Pitch computation ─────────────────────────────────────────────
    float basePitch = portoPitch_
                    + mod.pitchBendSemitones
                    + autoBendOffset_;

    // OSC1 pitch
    float osc1Semitones = basePitch
                        + octaveToSemitones(patch.osc1Octave)
                        + mod.mgOscSemitones;
    float osc1Hz = 440.0f * std::pow(2.0f, (osc1Semitones - 69.0f) / 12.0f);
    osc1_.setFrequency(osc1Hz);

    // OSC2 pitch
    float osc2Semitones = basePitch
                        + octaveToSemitones(patch.osc2Octave)
                        + intervalToSemitones(patch.osc2Interval)
                        + detuneToCents(patch.osc2Detune) / 100.0f
                        + mod.mgOscSemitones;
    float osc2Hz = 440.0f * std::pow(2.0f, (osc2Semitones - 69.0f) / 12.0f);
    osc2_.setFrequency(osc2Hz);

    // ── Mix oscillators ───────────────────────────────────────────────
    float osc1Lvl   = patch.osc1Level  / 31.0f;
    float osc2Lvl   = patch.osc2Level  / 31.0f;
    float noiseLvl  = patch.noiseLevel / 31.0f;

    float mixed = osc1_.processSample() * osc1Lvl
                + osc2_.processSample() * osc2Lvl
                + nextNoise()           * noiseLvl;

    // Prevent clipping before filter
    mixed *= 0.5f;

    // ── VCF EG ───────────────────────────────────────────────────────
    float egRaw  = vcfEG_.processSample();   // 0..1
    float polarity = (patch.vcfEGPolarity == 0) ? 1.0f : -1.0f;
    float egIntNorm = patch.vcfEGIntensity / 31.0f;

    // Apply EG in cutoff parameter space (0-63), matching DW-8000 hardware.
    // At full EG intensity, the envelope can sweep the filter from any base
    // cutoff all the way to maximum (63), which is ~110 semitones of range.
    float egDelta = polarity * egRaw * egIntNorm * 63.0f;
    float effectiveCutoff = std::clamp((float)patch.vcfCutoff + egDelta, 0.0f, 63.0f);

    // MG → VCF + aftertouch → VCF (still in semitones for small modulations)
    float atVcfSemitones = mod.aftertouch * (patch.atVcf / 3.0f) * 12.0f;
    float vcfBendSemitones = (patch.bendVcf != 0) ? mod.pitchBendVcfSemitones : 0.0f;

    vcf_.updateParams((uint8_t)effectiveCutoff, patch.vcfResonance,
                      patch.vcfKbdTrack, midiNote,
                      0.0f,                 // EG already baked into effectiveCutoff
                      mod.mgVcfSemitones,
                      atVcfSemitones,
                      vcfBendSemitones);

    float filtered = vcf_.processSample(mixed);

    // ── VCA ───────────────────────────────────────────────────────────
    float vcaEnv = vcaEG_.processSample();

    // Aftertouch → VCA
    float atVcaBoost = mod.aftertouch * (patch.atVca / 3.0f) * 0.3f;
    float amp = vcaEnv + atVcaBoost;

    float out = filtered * amp;

    // ── DC block ──────────────────────────────────────────────────────
    dcY_ = out - dcX_ + 0.9995f * dcY_;
    dcX_ = out;

    // Voice is silent after release
    if (vcaEG_.isIdle())
        active = false;

    return dcY_;
}
