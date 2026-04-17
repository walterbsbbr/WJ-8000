#pragma once
#include <vector>
#include <algorithm>
#include <cstdint>

// DW-8000 arpeggiator.
// Modes: UP/DOWN, ASSIGNABLE (order-played).
// Latch: holds up to 64 notes.
// Clock: internal (free-running) or MIDI clock sync.
class DW8000Arpeggiator
{
public:
    enum class Mode { UpDown = 0, Assignable = 1 };
    enum class Range { One = 0, Two = 1, Full = 2 };

    // ─── Config ────────────────────────────────────────────────────────
    bool  enabled    = false;
    Mode  mode       = Mode::UpDown;
    Range range      = Range::One;
    bool  latch      = false;
    int   clockParam = 3;   // 1–3 internal, 4–6 MIDI clock

    // Call once when sample rate changes
    void prepare(float sampleRate);

    // Notify arpeggiator of a note event (before arp processing)
    void noteOn (int note);
    void noteOff(int note);
    void allNotesOff();

    // speed: front-panel slider value 0.0–1.0
    void setSpeed(float speed);

    // Call once per audio sample. Returns true if a new arp step fired.
    // activeNote is set to the MIDI note that should play (-1 = none).
    bool processSample(int& activeNote, bool midiClockTick = false);

    // Trigger from external MIDI clock (call on each 0xF8 tick)
    void onMidiClock();

private:
    float sampleRate_    = 48000.0f;
    float samplesPerStep_= 4410.0f;  // ~120 BPM 8th note
    float sampleCount_   = 0.0f;

    std::vector<int> heldNotes_;    // currently held (not latched) notes
    std::vector<int> latchNotes_;   // latched sequence
    std::vector<int> sequence_;     // active arp sequence (built from above)

    int  stepIndex_     = 0;
    bool goingUp_       = true;     // for UP/DOWN mode
    int  currentNote_   = -1;

    int  midiClockCount_= 0;
    int  midiDivision_  = 6;       // MIDI clocks per step (6=32nd, 12=16th, 24=8th)

    void buildSequence();
    int  nextNote();
};
