#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

// ─── DW-8000 look-and-feel ────────────────────────────────────────────────
class DW8000LookAndFeel : public juce::LookAndFeel_V4
{
public:
    DW8000LookAndFeel();

    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float minPos, float maxPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;
};

// ─── Vintage piano keyboard display ──────────────────────────────────────
// C2 (MIDI 36) to C7 (MIDI 96) = 61 keys, 36 white keys.
// Active notes light up in blue; reads proc_.isNoteActive() from timerCallback.
class MiniKeyboard : public juce::Component
{
public:
    static constexpr int FIRST_NOTE = 36;  // C2
    static constexpr int LAST_NOTE  = 96;  // C7
    static constexpr int NUM_WHITE  = 36;  // 5 octaves × 7 + 1

    explicit MiniKeyboard(DW8000Processor& proc) : proc_(proc) {}
    void paint(juce::Graphics&) override;

private:
    DW8000Processor& proc_;
    static bool isBlackKey(int note) noexcept;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniKeyboard)
};

// ─── Red 7-segment LCD — shows current preset "11"–"88" ──────────────────
class PresetDisplay : public juce::Component
{
public:
    explicit PresetDisplay(DW8000Processor& proc) : proc_(proc) {}
    void paint(juce::Graphics&) override;

private:
    DW8000Processor& proc_;

    // Draw one 7-segment digit d (1–8) inside rect (x,y,w,h)
    static void drawDigit(juce::Graphics&, int d,
                          float x, float y, float w, float h,
                          juce::Colour lit, juce::Colour dim);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetDisplay)
};

// ─── Labelled vertical fader ─────────────────────────────────────────────
// Cell layout (top→bottom): 14 px name label | fader track | 18 px value box
struct Knob : public juce::Component
{
    juce::Slider slider { juce::Slider::LinearVertical,
                          juce::Slider::TextBoxBelow };
    juce::Label  label;
    std::unique_ptr<SliderAttachment> attachment;

    Knob(const juce::String& labelText,
         juce::AudioProcessorValueTreeState& apvts,
         const juce::String& paramID);

    void resized() override;
};

// ─── Main Editor ──────────────────────────────────────────────────────────
class DW8000Editor : public juce::AudioProcessorEditor,
                     public juce::FileDragAndDropTarget,
                     public juce::Timer
{
public:
    explicit DW8000Editor(DW8000Processor&);
    ~DW8000Editor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    DW8000Processor&  proc_;
    DW8000LookAndFeel laf_;
    bool draggingOver_ = false;

    // ── OSC 1 ─────────────────────────────────────────────────────────
    Knob kOsc1Oct,   kOsc1Wave, kOsc1Lvl;

    // ── Auto Bend ──────────────────────────────────────────────────────
    Knob kAutoBendSel, kAutoBendMode, kAutoBendTime, kAutoBendInt;

    // ── OSC 2 ─────────────────────────────────────────────────────────
    Knob kOsc2Oct,  kOsc2Wave, kOsc2Lvl, kOsc2Int, kOsc2Det;

    // ── Noise ─────────────────────────────────────────────────────────
    Knob kNoise;

    // ── MG / LFO ──────────────────────────────────────────────────────
    Knob kMgWav, kMgFreq, kMgDly, kMgOsc, kMgVcf;

    // ── VCF ───────────────────────────────────────────────────────────
    Knob kVcfCut, kVcfRes, kVcfKbd, kVcfEGPol, kVcfEGInt;
    Knob kVcfAtk, kVcfDec, kVcfBrk, kVcfSlp, kVcfSus, kVcfRel, kVcfVel;

    // ── VCA ───────────────────────────────────────────────────────────
    Knob kVcaAtk, kVcaDec, kVcaBrk, kVcaSlp, kVcaSus, kVcaRel, kVcaVel;

    // ── Key Assign radio buttons (p_0D: 0=POLY1 1=POLY2 2=UNISON1 3=UNISON2)
    juce::TextButton btnPoly1   { "POLY 1"   };
    juce::TextButton btnPoly2   { "POLY 2"   };
    juce::TextButton btnUnison1 { "UNISON 1" };
    juce::TextButton btnUnison2 { "UNISON 2" };

    // ── Voice / Misc ───────────────────────────────────────────────────
    Knob kPortoTime;

    // ── Bend ──────────────────────────────────────────────────────────
    Knob kBendRng, kBendVcf;

    // ── Aftertouch ────────────────────────────────────────────────────
    Knob kAtOsc, kAtVcf, kAtVca;

    // ── Delay ─────────────────────────────────────────────────────────
    Knob kDlyTim, kDlyFac, kDlyFbk, kDlyMFq, kDlyMIn, kDlyEfx;

    // ── Arpeggiator ───────────────────────────────────────────────────
    juce::TextButton btnArpOn    { "ON/OFF"   };
    juce::TextButton btnArpMode  { "ASSIGN"   };
    juce::TextButton btnArpLatch { "LATCH"    };
    juce::TextButton btnArpOct   { "OCT 1"    };
    Knob kArpSpeed, kArpClock;

    // ── Preset selector ───────────────────────────────────────────────
    PresetDisplay    presetDisplay_;
    juce::TextButton btnBank       { "BANK" };
    juce::TextButton btnProg       { "PROG" };
    juce::TextButton btnPresetUp   { "+"    };
    juce::TextButton btnPresetDown { "-"    };

    // ── Delay bypass ──────────────────────────────────────────────────
    juce::TextButton btnDelayBypass { "BYPASS" };

    // ── Buttons ────────────────────────────────────────────────────────
    juce::TextButton btnExport { "Export SYX" };
    juce::TextButton btnImport { "Import SYX" };
    juce::TextButton btnPanic  { "PANIC" };

    MiniKeyboard keyboard_;

    void setKeyAssignMode(int mode);   // 0-3, updates param + button states
    void updateKeyAssignButtons();     // reads param, refreshes button look
    void updateArpButtons();           // reads arp params, refreshes button look

    void exportSyx();
    void importSyx();

    // Layout helpers
    static void lay(juce::Rectangle<int> r, std::initializer_list<Knob*> knobs);
    static void sect(juce::Graphics& g, const char* title, juce::Rectangle<int> r);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DW8000Editor)
};
