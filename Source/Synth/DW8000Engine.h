#pragma once
#include "DW8000Voice.h"
#include "DW8000MG.h"
#include "DW8000Delay.h"
#include "DW8000Arpeggiator.h"
#include "DWGS/DWGSWavetableBank.h"
#include "DW8000Patch.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

class DW8000Engine
{
public:
    static constexpr int NUM_VOICES = 8;

    // ── Lifecycle ──────────────────────────────────────────────────────
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

    // ── Patch management ───────────────────────────────────────────────
    void setPatch(const DW8000Patch& patch);
    void applyParamChange(uint8_t offset, uint8_t value);
    const DW8000Patch& getPatch() const { return patch_; }

    // ── Audio render ───────────────────────────────────────────────────
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages);

    // ── Arpeggiator control ────────────────────────────────────────────
    void setArpParams(bool on, int mode, bool latch, int octave, int clock, int speed);

    // ── Panic / bypass ────────────────────────────────────────────────
    void panicAllNotesOff();          // release all voices immediately
    void setDelayBypassed(bool b) noexcept { delayBypassed_ = b; }

    // ── Wavetable bank access (call loadFromBinaryData after construction) ──
    DWGSWavetableBank& getWavetableBank() { return waveBank_; }

private:
    float sampleRate_ = 48000.0f;

    DWGSWavetableBank          waveBank_;
    std::array<DW8000Voice, NUM_VOICES> voices_;
    DW8000MG                   mg_;
    DW8000Delay                delay_;
    DW8000Arpeggiator          arp_;
    DW8000Patch                patch_;

    // Global performance state
    float pitchBendSemitones_ = 0.0f;
    float aftertouch_         = 0.0f;
    bool  delayBypassed_      = false;

    // Arp state
    int   currentArpNote_  = -1;
    float lastArpVelocity_ = 1.0f;

    // ── MIDI handling ──────────────────────────────────────────────────
    void handleNoteOn  (int channel, int note, float velocity);
    void handleNoteOff (int channel, int note);
    void handlePitchBend(int channel, int value14bit);
    void handleAftertouch(int channel, int value);
    void handleCC(int channel, int cc, int value);

    // ── Voice allocation ───────────────────────────────────────────────
    void allocatePoly(int note, float vel);
    void allocateUnison(int note, float vel);
    int  findFreeVoice()  const;
    int  stealVoice()     const;

    void releaseVoicesForNote(int note);

    // ── Helpers ────────────────────────────────────────────────────────
    void applyPatchToAllVoices();
};
