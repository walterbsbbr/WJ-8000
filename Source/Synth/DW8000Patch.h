#pragma once
#include <cstdint>
#include <array>
#include <string>

// Complete 51-parameter DW-8000 patch.
// Byte offsets match the real SysEx data dump (F0 42 3n 03 40 [51 bytes] F7).
struct DW8000Patch
{
    // ── OSC 1 ────────────────────────────────────────── offsets 0x00–0x06
    uint8_t osc1Octave        = 1;   // 0=16', 1=8', 2=4'
    uint8_t osc1Waveform      = 0;   // 0–15  (panel 1–16)
    uint8_t osc1Level         = 31;  // 0–31
    uint8_t autoBendSelect    = 0;   // 0=Off,1=OSC1,2=OSC2,3=Both
    uint8_t autoBendMode      = 0;   // 0=Up, 1=Down
    uint8_t autoBendTime      = 0;   // 0–31
    uint8_t autoBendIntensity = 0;   // 0–31

    // ── OSC 2 ────────────────────────────────────────── offsets 0x07–0x0C
    uint8_t osc2Octave   = 1;   // 0=16', 1=8', 2=4'
    uint8_t osc2Waveform = 0;   // 0–15
    uint8_t osc2Level    = 0;   // 0–31
    uint8_t osc2Interval = 0;   // 0=Unison,1=min3,2=Maj3,3=P4,4=P5
    uint8_t osc2Detune   = 0;   // 0–6 (~25 cents max)
    uint8_t noiseLevel   = 0;   // 0–31

    // ── Global (stored in patch) ──────────────────────── offsets 0x0D–0x0E
    uint8_t keyAssignMode = 0;  // 0=Poly1,1=Poly2,2=Uni1,3=Uni2
    uint8_t paramNumMemo  = 0;  // last-edited param (internal DW use)

    // ── VCF ──────────────────────────────────────────── offsets 0x0F–0x1A
    uint8_t vcfCutoff      = 40;
    uint8_t vcfResonance   = 0;
    uint8_t vcfKbdTrack    = 0;   // 0=0, 1=1/4, 2=1/2, 3=1
    uint8_t vcfEGPolarity  = 0;   // 0=Positive, 1=Negative
    uint8_t vcfEGIntensity = 20;  // 0–31
    uint8_t vcfAttack      = 5;
    uint8_t vcfDecay       = 15;
    uint8_t vcfBreakPoint  = 20;
    uint8_t vcfSlopeTime   = 10;
    uint8_t vcfSustain     = 15;
    uint8_t vcfRelease     = 10;
    uint8_t vcfVelSens     = 3;   // 0–7

    // ── VCA ──────────────────────────────────────────── offsets 0x1B–0x21
    uint8_t vcaAttack     = 3;
    uint8_t vcaDecay      = 15;
    uint8_t vcaBreakPoint = 25;
    uint8_t vcaSlopeTime  = 10;
    uint8_t vcaSustain    = 20;
    uint8_t vcaRelease    = 12;
    uint8_t vcaVelSens    = 3;

    // ── MG (LFO) ─────────────────────────────────────── offsets 0x22–0x26
    uint8_t mgWaveform = 0;   // 0=Tri,1=UpSaw,2=DnSaw,3=Sq
    uint8_t mgFrequency = 10; // 0–31
    uint8_t mgDelay    = 0;   // 0–31
    uint8_t mgOscDepth = 0;   // 0–31  vibrato
    uint8_t mgVcfDepth = 0;   // 0–31  filter mod

    // ── Bend ─────────────────────────────────────────── offsets 0x27–0x28
    uint8_t bendOscRange = 2;  // semitones 0–12
    uint8_t bendVcf      = 0;  // 0=Off,1=On

    // ── Delay ────────────────────────────────────────── offsets 0x29–0x2E
    uint8_t delayTime        = 0;  // 0–7
    uint8_t delayFactor      = 0;  // 0–15
    uint8_t delayFeedback    = 0;  // 0–15
    uint8_t delayModFreq     = 0;  // 0–31
    uint8_t delayModIntensity= 0;  // 0–31
    uint8_t delayEffectLevel = 0;  // 0–15

    // ── Portamento ───────────────────────────────────── offset  0x2F
    uint8_t portamentoTime = 0;  // 0–31

    // ── Aftertouch ───────────────────────────────────── offsets 0x30–0x32
    uint8_t atOscMG = 0;  // 0–3
    uint8_t atVcf   = 0;  // 0–3
    uint8_t atVca   = 0;  // 0–3

    // ─────────────────────────────────────────────────────────────────────
    std::array<uint8_t, 51> toSysExBytes() const;
    static DW8000Patch fromSysExBytes(const uint8_t* data);
    void applyParamChange(uint8_t offset, uint8_t value);
    void clamp();   // enforce hardware value ranges

    static DW8000Patch makeInit();
};
