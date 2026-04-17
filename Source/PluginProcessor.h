#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Synth/DW8000Engine.h"
#include "MIDI/DW8000SysEx.h"
#include <atomic>

class DW8000Processor : public juce::AudioProcessor
{
public:
    DW8000Processor();
    ~DW8000Processor() override = default;

    // ── AudioProcessor ────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "DW-8000"; }
    bool acceptsMidi()  const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    // ── Program / Preset bank (8 banks × 8 programs = 64 slots) ─────────
    // Slot index 0–63 maps to display numbers 11–18, 21–28 … 81–88
    static constexpr int NUM_PRESETS = 64;

    int  getNumPrograms()    override { return NUM_PRESETS; }
    int  getCurrentProgram() override { return currentPreset_; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& name) override;

    // Load slot → engine+APVTS; write engine → slot
    void loadPreset(int index);
    void writeCurrentToPreset(int index);

    // Bank .syx I/O
    // Supports two formats:
    //   • 64 × 57 bytes  — raw DataDump concatenation
    //   • 64 × 66 bytes  — hardware dump: [C0 prog][57-byte DataDump][7-byte WriteReq]
    bool loadBankFromFile(const juce::File& f);
    bool saveBankToFile  (const juce::File& f) const;

    // Load the embedded factory bank (called once from constructor)
    void loadFactoryBank();

    void getStateInformation(juce::MemoryBlock& dest) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    // ── Patch access (called from editor) ────────────────────────────
    DW8000Patch getCurrentPatch() const;
    void         loadPatch(const DW8000Patch& p);

    // ── Panic: release all sounding notes immediately ─────────────────
    void panicAllNotesOff() { engine_.panicAllNotesOff(); }

    // APVTS — one parameter per SysEx offset (offsets 0–50)
    juce::AudioProcessorValueTreeState apvts;

    // Expose engine for editor inspection
    DW8000Engine& getEngine() { return engine_; }

    // ── Active note display (for keyboard visualisation) ──────────────
    // Updated from processBlock (audio thread) — read from GUI thread.
    bool isNoteActive(int note) const noexcept
    {
        if ((unsigned)note >= 128) return false;
        if (note < 64)
            return (activeNotesLo_.load(std::memory_order_relaxed) >> note) & 1;
        return (activeNotesHi_.load(std::memory_order_relaxed) >> (note - 64)) & 1;
    }

private:
    DW8000Engine engine_;

    // ── Preset bank ───────────────────────────────────────────────────────
    DW8000Patch  presetBank_[NUM_PRESETS];
    juce::String presetNames_[NUM_PRESETS];
    int          currentPreset_ = 0;

    // Returns display string "11"–"88" for a slot index 0–63
    static juce::String slotToDisplayName(int index);
    // Initialises bank with factory defaults
    void initBankDefaults();

    // Outgoing SysEx MIDI for hardware sync
    juce::MidiBuffer pendingMidi_;
    int midiChannel_ = 0;  // 0-based

    // Active note bitmask (notes 0-63 / 64-127)
    std::atomic<uint64_t> activeNotesLo_{ 0 };
    std::atomic<uint64_t> activeNotesHi_{ 0 };

    // Handle incoming SysEx in processBlock
    void handleSysEx(const juce::MidiMessage& msg);

    // Build the APVTS parameter layout (51 params)
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // Sync APVTS → patch (audio thread safe via param smoothing)
    void syncParamsToEngine();

    juce::ListenerList<juce::AudioProcessorValueTreeState::Listener> listeners_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DW8000Processor)
};
