#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Synth/DW8000Patch.h"

// Full Korg DW-8000 SysEx protocol implementation.
// Format:  F0 42 3n 03 <cmd> [data] F7   (n = MIDI channel 0-15)
class DW8000SysEx
{
public:
    static constexpr uint8_t KORG_ID    = 0x42;
    static constexpr uint8_t DEVICE_ID  = 0x03;
    static constexpr int     DUMP_SIZE  = 51;   // bytes in a patch dump payload

    enum class Command : uint8_t
    {
        DataSaveRequest = 0x10,
        WriteRequest    = 0x11,
        WriteComplete   = 0x21,
        WriteError      = 0x22,
        DataDump        = 0x40,
        ParamChange     = 0x41,
    };

    struct ParseResult
    {
        bool      valid        = false;
        Command   command      = Command::DataDump;
        DW8000Patch patch;        // populated for DataDump
        uint8_t   paramOffset  = 0;
        uint8_t   paramValue   = 0;
        uint8_t   programNum   = 0;   // for WriteRequest (0–63)
        int       midiChannel  = 0;   // 0-based
    };

    // ── Parse any incoming SysEx message ────────────────────────────────
    static ParseResult parse(const juce::MidiMessage& msg);

    // ── Build outgoing messages ──────────────────────────────────────────
    static juce::MidiMessage makeDataDump(const DW8000Patch& p, int midiCh = 0);
    static juce::MidiMessage makeParamChange(uint8_t offset, uint8_t value, int midiCh = 0);
    static juce::MidiMessage makeDataSaveRequest(int midiCh = 0);
    static juce::MidiMessage makeWriteRequest(uint8_t programNum, int midiCh = 0);
    static juce::MidiMessage makeWriteComplete(int midiCh = 0);
    static juce::MidiMessage makeDeviceIdResponse(int midiCh = 0);

    // ── Helpers ──────────────────────────────────────────────────────────
    static bool isValidDW8000Dump(const uint8_t* data, int size);
};
