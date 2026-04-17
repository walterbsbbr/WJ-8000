#include "DW8000Patch.h"
#include <algorithm>

// ─── Serialize to 51-byte SysEx payload ───────────────────────────────────
std::array<uint8_t, 51> DW8000Patch::toSysExBytes() const
{
    std::array<uint8_t, 51> b{};
    b[0x00] = osc1Octave        & 0x03;
    b[0x01] = osc1Waveform      & 0x0F;
    b[0x02] = osc1Level         & 0x1F;
    b[0x03] = autoBendSelect    & 0x03;
    b[0x04] = autoBendMode      & 0x01;
    b[0x05] = autoBendTime      & 0x1F;
    b[0x06] = autoBendIntensity & 0x1F;
    b[0x07] = osc2Octave        & 0x03;
    b[0x08] = osc2Waveform      & 0x0F;
    b[0x09] = osc2Level         & 0x1F;
    b[0x0A] = osc2Interval      & 0x07;
    b[0x0B] = osc2Detune        & 0x07;
    b[0x0C] = noiseLevel        & 0x1F;
    b[0x0D] = keyAssignMode     & 0x03;
    b[0x0E] = paramNumMemo      & 0x3F;
    b[0x0F] = vcfCutoff         & 0x3F;
    b[0x10] = vcfResonance      & 0x1F;
    b[0x11] = vcfKbdTrack       & 0x03;
    b[0x12] = vcfEGPolarity     & 0x01;
    b[0x13] = vcfEGIntensity    & 0x1F;
    b[0x14] = vcfAttack         & 0x1F;
    b[0x15] = vcfDecay          & 0x1F;
    b[0x16] = vcfBreakPoint     & 0x1F;
    b[0x17] = vcfSlopeTime      & 0x1F;
    b[0x18] = vcfSustain        & 0x1F;
    b[0x19] = vcfRelease        & 0x1F;
    b[0x1A] = vcfVelSens        & 0x07;
    b[0x1B] = vcaAttack         & 0x1F;
    b[0x1C] = vcaDecay          & 0x1F;
    b[0x1D] = vcaBreakPoint     & 0x1F;
    b[0x1E] = vcaSlopeTime      & 0x1F;
    b[0x1F] = vcaSustain        & 0x1F;
    b[0x20] = vcaRelease        & 0x1F;
    b[0x21] = vcaVelSens        & 0x07;
    b[0x22] = mgWaveform        & 0x03;
    b[0x23] = mgFrequency       & 0x1F;
    b[0x24] = mgDelay           & 0x1F;
    b[0x25] = mgOscDepth        & 0x1F;
    b[0x26] = mgVcfDepth        & 0x1F;
    b[0x27] = bendOscRange      & 0x0F;
    b[0x28] = bendVcf           & 0x01;
    b[0x29] = delayTime         & 0x07;
    b[0x2A] = delayFactor       & 0x0F;
    b[0x2B] = delayFeedback     & 0x0F;
    b[0x2C] = delayModFreq      & 0x1F;
    b[0x2D] = delayModIntensity & 0x1F;
    b[0x2E] = delayEffectLevel  & 0x0F;
    b[0x2F] = portamentoTime    & 0x1F;
    b[0x30] = atOscMG           & 0x03;
    b[0x31] = atVcf             & 0x03;
    b[0x32] = atVca             & 0x03;
    return b;
}

// ─── Deserialize from 51-byte SysEx payload ───────────────────────────────
DW8000Patch DW8000Patch::fromSysExBytes(const uint8_t* b)
{
    DW8000Patch p;
    p.osc1Octave        = b[0x00] & 0x03;
    p.osc1Waveform      = b[0x01] & 0x0F;
    p.osc1Level         = b[0x02] & 0x1F;
    p.autoBendSelect    = b[0x03] & 0x03;
    p.autoBendMode      = b[0x04] & 0x01;
    p.autoBendTime      = b[0x05] & 0x1F;
    p.autoBendIntensity = b[0x06] & 0x1F;
    p.osc2Octave        = b[0x07] & 0x03;
    p.osc2Waveform      = b[0x08] & 0x0F;
    p.osc2Level         = b[0x09] & 0x1F;
    p.osc2Interval      = b[0x0A] & 0x07;
    p.osc2Detune        = b[0x0B] & 0x07;
    p.noiseLevel        = b[0x0C] & 0x1F;
    p.keyAssignMode     = b[0x0D] & 0x03;
    p.paramNumMemo      = b[0x0E] & 0x3F;
    p.vcfCutoff         = b[0x0F] & 0x3F;
    p.vcfResonance      = b[0x10] & 0x1F;
    p.vcfKbdTrack       = b[0x11] & 0x03;
    p.vcfEGPolarity     = b[0x12] & 0x01;
    p.vcfEGIntensity    = b[0x13] & 0x1F;
    p.vcfAttack         = b[0x14] & 0x1F;
    p.vcfDecay          = b[0x15] & 0x1F;
    p.vcfBreakPoint     = b[0x16] & 0x1F;
    p.vcfSlopeTime      = b[0x17] & 0x1F;
    p.vcfSustain        = b[0x18] & 0x1F;
    p.vcfRelease        = b[0x19] & 0x1F;
    p.vcfVelSens        = b[0x1A] & 0x07;
    p.vcaAttack         = b[0x1B] & 0x1F;
    p.vcaDecay          = b[0x1C] & 0x1F;
    p.vcaBreakPoint     = b[0x1D] & 0x1F;
    p.vcaSlopeTime      = b[0x1E] & 0x1F;
    p.vcaSustain        = b[0x1F] & 0x1F;
    p.vcaRelease        = b[0x20] & 0x1F;
    p.vcaVelSens        = b[0x21] & 0x07;
    p.mgWaveform        = b[0x22] & 0x03;
    p.mgFrequency       = b[0x23] & 0x1F;
    p.mgDelay           = b[0x24] & 0x1F;
    p.mgOscDepth        = b[0x25] & 0x1F;
    p.mgVcfDepth        = b[0x26] & 0x1F;
    p.bendOscRange      = b[0x27] & 0x0F;
    p.bendVcf           = b[0x28] & 0x01;
    p.delayTime         = b[0x29] & 0x07;
    p.delayFactor       = b[0x2A] & 0x0F;
    p.delayFeedback     = b[0x2B] & 0x0F;
    p.delayModFreq      = b[0x2C] & 0x1F;
    p.delayModIntensity = b[0x2D] & 0x1F;
    p.delayEffectLevel  = b[0x2E] & 0x0F;
    p.portamentoTime    = b[0x2F] & 0x1F;
    p.atOscMG           = b[0x30] & 0x03;
    p.atVcf             = b[0x31] & 0x03;
    p.atVca             = b[0x32] & 0x03;
    return p;
}

// ─── Apply single real-time parameter change ──────────────────────────────
void DW8000Patch::applyParamChange(uint8_t offset, uint8_t value)
{
    auto bytes = toSysExBytes();
    if (offset < 51)
    {
        bytes[offset] = value;
        *this = fromSysExBytes(bytes.data());
    }
}

// ─── Clamp all fields to hardware spec ranges ─────────────────────────────
void DW8000Patch::clamp()
{
    auto c = [](uint8_t& v, uint8_t hi) { v = std::min(v, hi); };
    c(osc1Octave, 2); c(osc1Waveform, 15); c(osc1Level, 31);
    c(autoBendSelect, 3); c(autoBendMode, 1);
    c(autoBendTime, 31); c(autoBendIntensity, 31);
    c(osc2Octave, 2); c(osc2Waveform, 15); c(osc2Level, 31);
    c(osc2Interval, 4); c(osc2Detune, 6); c(noiseLevel, 31);
    c(keyAssignMode, 3);
    c(vcfCutoff, 63); c(vcfResonance, 31);
    c(vcfKbdTrack, 3); c(vcfEGPolarity, 1); c(vcfEGIntensity, 31);
    c(vcfAttack, 31); c(vcfDecay, 31); c(vcfBreakPoint, 31);
    c(vcfSlopeTime, 31); c(vcfSustain, 31); c(vcfRelease, 31); c(vcfVelSens, 7);
    c(vcaAttack, 31); c(vcaDecay, 31); c(vcaBreakPoint, 31);
    c(vcaSlopeTime, 31); c(vcaSustain, 31); c(vcaRelease, 31); c(vcaVelSens, 7);
    c(mgWaveform, 3); c(mgFrequency, 31); c(mgDelay, 31);
    c(mgOscDepth, 31); c(mgVcfDepth, 31);
    c(bendOscRange, 12); c(bendVcf, 1);
    c(delayTime, 7); c(delayFactor, 15); c(delayFeedback, 15);
    c(delayModFreq, 31); c(delayModIntensity, 31); c(delayEffectLevel, 15);
    c(portamentoTime, 31);
    c(atOscMG, 3); c(atVcf, 3); c(atVca, 3);
}

DW8000Patch DW8000Patch::makeInit()
{
    return DW8000Patch{};  // default member values = a usable init patch
}
