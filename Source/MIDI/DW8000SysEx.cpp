#include "DW8000SysEx.h"

bool DW8000SysEx::isValidDW8000Dump(const uint8_t* d, int size)
{
    // F0 42 3n 03 40 [51 bytes] F7 = 57 bytes total
    return size == 57
        && d[0]  == 0xF0
        && d[1]  == KORG_ID
        && (d[2] & 0xF0) == 0x30   // 3n  (format ID)
        && d[3]  == DEVICE_ID
        && d[4]  == static_cast<uint8_t>(Command::DataDump)
        && d[56] == 0xF7;
}

// ─── Parse ────────────────────────────────────────────────────────────────
DW8000SysEx::ParseResult DW8000SysEx::parse(const juce::MidiMessage& msg)
{
    ParseResult r;
    if (!msg.isSysEx()) return r;

    const uint8_t* d    = msg.getSysExData();
    int            size = msg.getSysExDataSize() + 2;  // JUCE strips F0/F7 from getSysExData
    // Recheck using raw data
    const uint8_t* raw  = reinterpret_cast<const uint8_t*>(msg.getRawData());
    int            rawSz= msg.getRawDataSize();

    if (rawSz < 5) return r;
    if (raw[0] != 0xF0)        return r;
    if (raw[1] != KORG_ID)     return r;
    if ((raw[2] & 0xF0) != 0x30) return r;
    if (raw[3] != DEVICE_ID)   return r;

    r.midiChannel = raw[2] & 0x0F;
    uint8_t cmd   = raw[4];

    if (cmd == static_cast<uint8_t>(Command::DataDump) && rawSz == 57)
    {
        if (raw[rawSz - 1] != 0xF7) return r;
        r.valid   = true;
        r.command = Command::DataDump;
        r.patch   = DW8000Patch::fromSysExBytes(raw + 5);
        return r;
    }
    if (cmd == static_cast<uint8_t>(Command::ParamChange) && rawSz == 8)
    {
        r.valid       = true;
        r.command     = Command::ParamChange;
        r.paramOffset = raw[5];
        r.paramValue  = raw[6];
        return r;
    }
    if (cmd == static_cast<uint8_t>(Command::DataSaveRequest))
    {
        r.valid   = true;
        r.command = Command::DataSaveRequest;
        return r;
    }
    if (cmd == static_cast<uint8_t>(Command::WriteRequest) && rawSz == 8)
    {
        r.valid      = true;
        r.command    = Command::WriteRequest;
        r.programNum = raw[5];
        return r;
    }
    return r;
}

// ─── Build messages ───────────────────────────────────────────────────────
juce::MidiMessage DW8000SysEx::makeDataDump(const DW8000Patch& p, int midiCh)
{
    // F0 42 3n 03 40 [51 bytes] F7  → 57 bytes total
    uint8_t buf[57];
    buf[0] = 0xF0;
    buf[1] = KORG_ID;
    buf[2] = 0x30 | (midiCh & 0x0F);
    buf[3] = DEVICE_ID;
    buf[4] = static_cast<uint8_t>(Command::DataDump);
    auto bytes = p.toSysExBytes();
    std::copy(bytes.begin(), bytes.end(), buf + 5);
    buf[56] = 0xF7;
    return juce::MidiMessage(buf, 57);
}

juce::MidiMessage DW8000SysEx::makeParamChange(uint8_t offset, uint8_t value, int midiCh)
{
    // F0 42 3n 03 41 [offset] [value] F7  → 8 bytes
    uint8_t buf[8] = { 0xF0, KORG_ID,
                       (uint8_t)(0x30 | (midiCh & 0x0F)),
                       DEVICE_ID,
                       static_cast<uint8_t>(Command::ParamChange),
                       offset, value, 0xF7 };
    return juce::MidiMessage(buf, 8);
}

juce::MidiMessage DW8000SysEx::makeDataSaveRequest(int midiCh)
{
    uint8_t buf[6] = { 0xF0, KORG_ID,
                       (uint8_t)(0x30 | (midiCh & 0x0F)),
                       DEVICE_ID,
                       static_cast<uint8_t>(Command::DataSaveRequest),
                       0xF7 };
    return juce::MidiMessage(buf, 6);
}

juce::MidiMessage DW8000SysEx::makeWriteRequest(uint8_t programNum, int midiCh)
{
    uint8_t buf[8] = { 0xF0, KORG_ID,
                       (uint8_t)(0x30 | (midiCh & 0x0F)),
                       DEVICE_ID,
                       static_cast<uint8_t>(Command::WriteRequest),
                       (uint8_t)(programNum & 0x3F), 0x00, 0xF7 };
    return juce::MidiMessage(buf, 8);
}

juce::MidiMessage DW8000SysEx::makeWriteComplete(int midiCh)
{
    uint8_t buf[6] = { 0xF0, KORG_ID,
                       (uint8_t)(0x30 | (midiCh & 0x0F)),
                       DEVICE_ID,
                       static_cast<uint8_t>(Command::WriteComplete),
                       0xF7 };
    return juce::MidiMessage(buf, 6);
}

juce::MidiMessage DW8000SysEx::makeDeviceIdResponse(int midiCh)
{
    uint8_t buf[5] = { 0xF0, KORG_ID,
                       (uint8_t)(0x30 | (midiCh & 0x0F)),
                       DEVICE_ID, 0xF7 };
    return juce::MidiMessage(buf, 5);
}
