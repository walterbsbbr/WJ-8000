#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DW8000FactoryData.h"

// ─── Parameter layout ─────────────────────────────────────────────────────
// Each SysEx parameter offset maps to one APVTS parameter.
// Name format: "p_XX"  where XX = hex offset 00–32

static const struct { const char* id; const char* name; int lo; int hi; int def; } kParams[] =
{
    {"p_00","OSC1 Octave",         0, 2,  1},
    {"p_01","OSC1 Waveform",       0,15,  0},
    {"p_02","OSC1 Level",          0,31, 31},
    {"p_03","Auto Bend Select",    0, 3,  0},
    {"p_04","Auto Bend Mode",      0, 1,  0},
    {"p_05","Auto Bend Time",      0,31,  0},
    {"p_06","Auto Bend Intensity", 0,31,  0},
    {"p_07","OSC2 Octave",         0, 2,  1},
    {"p_08","OSC2 Waveform",       0,15,  0},
    {"p_09","OSC2 Level",          0,31,  0},
    {"p_0A","OSC2 Interval",       0, 4,  0},
    {"p_0B","OSC2 Detune",         0, 6,  0},
    {"p_0C","Noise Level",         0,31,  0},
    {"p_0D","Key Assign Mode",     0, 3,  0},
    {"p_0E","Param Memo",          0,62,  0},
    {"p_0F","VCF Cutoff",          0,63, 40},
    {"p_10","VCF Resonance",       0,31,  0},
    {"p_11","VCF KBD Track",       0, 3,  0},
    {"p_12","VCF EG Polarity",     0, 1,  0},
    {"p_13","VCF EG Intensity",    0,31, 20},
    {"p_14","VCF Attack",          0,31,  5},
    {"p_15","VCF Decay",           0,31, 15},
    {"p_16","VCF Break Point",     0,31, 20},
    {"p_17","VCF Slope Time",      0,31, 10},
    {"p_18","VCF Sustain",         0,31, 15},
    {"p_19","VCF Release",         0,31, 10},
    {"p_1A","VCF Vel Sens",        0, 7,  3},
    {"p_1B","VCA Attack",          0,31,  3},
    {"p_1C","VCA Decay",           0,31, 15},
    {"p_1D","VCA Break Point",     0,31, 25},
    {"p_1E","VCA Slope Time",      0,31, 10},
    {"p_1F","VCA Sustain",         0,31, 20},
    {"p_20","VCA Release",         0,31, 12},
    {"p_21","VCA Vel Sens",        0, 7,  3},
    {"p_22","MG Waveform",         0, 3,  0},
    {"p_23","MG Frequency",        0,31, 10},
    {"p_24","MG Delay",            0,31,  0},
    {"p_25","MG OSC Depth",        0,31,  0},
    {"p_26","MG VCF Depth",        0,31,  0},
    {"p_27","Bend OSC Range",      0,12,  2},
    {"p_28","Bend VCF",            0, 1,  0},
    {"p_29","Delay Time",          0, 7,  0},
    {"p_2A","Delay Factor",        0,15,  0},
    {"p_2B","Delay Feedback",      0,15,  0},
    {"p_2C","Delay Mod Freq",      0,31,  0},
    {"p_2D","Delay Mod Intensity", 0,31,  0},
    {"p_2E","Delay Effect Level",  0,15,  0},
    {"p_2F","Portamento Time",     0,31,  0},
    {"p_30","AT OSC MG",           0, 3,  0},
    {"p_31","AT VCF",              0, 3,  0},
    {"p_32","AT VCA",              0, 3,  0},
};
static constexpr int NUM_PARAMS = 51;

juce::AudioProcessorValueTreeState::ParameterLayout DW8000Processor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        layout.add(std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID{ kParams[i].id, 1 },
            kParams[i].name,
            kParams[i].lo,
            kParams[i].hi,
            kParams[i].def));
    }

    // ── Arpeggiator parameters (not part of SysEx patch) ──────────────
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"arp_on",    1}, "Arp On",     false));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"arp_mode",  1}, "Arp Mode",   0, 1, 0));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"arp_latch", 1}, "Arp Latch",  false));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"arp_octave",1}, "Arp Octave", 0, 2, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"arp_clock", 1}, "Arp Clock",  1, 6, 3));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"arp_speed", 1}, "Arp Speed",  0, 127, 64));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"delay_bypass", 1}, "Delay Bypass", false));

    return layout;
}

// ─── Preset bank helpers ──────────────────────────────────────────────────
juce::String DW8000Processor::slotToDisplayName(int index)
{
    // index 0–63 → "11"–"88"
    int bank = index / 8 + 1;
    int prog = index % 8 + 1;
    return juce::String(bank * 10 + prog);
}

void DW8000Processor::initBankDefaults()
{
    for (int i = 0; i < NUM_PRESETS; ++i)
    {
        presetBank_[i]  = DW8000Patch{};   // default-constructed patch
        presetNames_[i] = slotToDisplayName(i);
    }
}

// ─── Factory bank ─────────────────────────────────────────────────────────
// Hardware .syx format: each patch = [C0 prog] [57-byte DataDump] [7-byte WriteReq]
static bool parseBankFrom66(const uint8_t* raw, int size,
                             DW8000Patch (&bank)[DW8000Processor::NUM_PRESETS])
{
    if (size != DW8000Processor::NUM_PRESETS * 66) return false;
    for (int i = 0; i < DW8000Processor::NUM_PRESETS; ++i)
    {
        const uint8_t* dump = raw + i * 66 + 2;   // skip 2-byte MIDI PC message
        if (!DW8000SysEx::isValidDW8000Dump(dump, 57)) return false;
        bank[i] = DW8000Patch::fromSysExBytes(dump + 5);  // skip 5-byte SysEx header
    }
    return true;
}

void DW8000Processor::loadFactoryBank()
{
    const auto* raw  = reinterpret_cast<const uint8_t*>(DW8000FactoryData::FactoryBank_syx);
    const int   size = DW8000FactoryData::FactoryBank_syxSize;

    if (parseBankFrom66(raw, size, presetBank_))
        loadPreset(0);   // start on preset 11
}

// ─── Constructor ──────────────────────────────────────────────────────────
DW8000Processor::DW8000Processor()
    : AudioProcessor(BusesProperties()
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "DW8000State", createLayout())
{
    initBankDefaults();
    loadFactoryBank();   // populate bank with factory sounds on every cold open
}

// ─── Prepare ──────────────────────────────────────────────────────────────
void DW8000Processor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine_.prepare(sampleRate, samplesPerBlock);
    syncParamsToEngine();
}

// ─── Sync APVTS → engine (called after prepare or param change) ───────────
void DW8000Processor::syncParamsToEngine()
{
    DW8000Patch p;
    auto bytes = p.toSysExBytes();  // get default first

    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        auto* param = dynamic_cast<juce::AudioParameterInt*>(
            apvts.getParameter(kParams[i].id));
        if (param)
            bytes[i] = (uint8_t)param->get();
    }

    engine_.setPatch(DW8000Patch::fromSysExBytes(bytes.data()));

    // ── Arp params ────────────────────────────────────────────────────
    auto* pOn    = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("arp_on"));
    auto* pMode  = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter("arp_mode"));
    auto* pLatch = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("arp_latch"));
    auto* pOct   = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter("arp_octave"));
    auto* pClock = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter("arp_clock"));
    auto* pSpeed = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter("arp_speed"));

    engine_.setArpParams(
        pOn    ? pOn->get()    : false,
        pMode  ? pMode->get()  : 0,
        pLatch ? pLatch->get() : false,
        pOct   ? pOct->get()   : 0,
        pClock ? pClock->get() : 3,
        pSpeed ? pSpeed->get() : 64);

    // ── Delay bypass ──────────────────────────────────────────────────
    auto* pBypass = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("delay_bypass"));
    engine_.setDelayBypassed(pBypass && pBypass->get());
}

// ─── Process ──────────────────────────────────────────────────────────────
void DW8000Processor::processBlock(juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Scan MIDI: handle SysEx and maintain active-note bitmask for keyboard display
    for (const auto meta : midiMessages)
    {
        const auto& msg = meta.getMessage();

        if (msg.isSysEx())
        {
            handleSysEx(msg);
            continue;
        }

        int n = msg.getNoteNumber();
        auto setNote = [&]()
        {
            if (n < 64) activeNotesLo_.fetch_or (uint64_t(1) << n,        std::memory_order_relaxed);
            else        activeNotesHi_.fetch_or (uint64_t(1) << (n - 64), std::memory_order_relaxed);
        };
        auto clrNote = [&]()
        {
            if (n < 64) activeNotesLo_.fetch_and(~(uint64_t(1) << n),        std::memory_order_relaxed);
            else        activeNotesHi_.fetch_and(~(uint64_t(1) << (n - 64)), std::memory_order_relaxed);
        };

        if (msg.isNoteOn() && msg.getVelocity() > 0)        setNote();
        else if (msg.isNoteOff())                            clrNote();
        else if (msg.isNoteOn() && msg.getVelocity() == 0)  clrNote();  // vel-0 = note off
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            activeNotesLo_.store(0, std::memory_order_relaxed);
            activeNotesHi_.store(0, std::memory_order_relaxed);
        }
    }

    // Sync APVTS → engine every block (reading 51 atomics is cheap)
    syncParamsToEngine();

    engine_.processBlock(buffer, midiMessages);
}

void DW8000Processor::handleSysEx(const juce::MidiMessage& msg)
{
    auto result = DW8000SysEx::parse(msg);
    if (!result.valid) return;

    if (result.command == DW8000SysEx::Command::DataDump)
    {
        loadPatch(result.patch);
    }
    else if (result.command == DW8000SysEx::Command::ParamChange)
    {
        engine_.applyParamChange(result.paramOffset, result.paramValue);
        // Sync back to APVTS (so GUI updates)
        if (result.paramOffset < NUM_PARAMS)
        {
            auto* p = dynamic_cast<juce::AudioParameterInt*>(
                apvts.getParameter(kParams[result.paramOffset].id));
            if (p) *p = result.paramValue;
        }
    }
    else if (result.command == DW8000SysEx::Command::DataSaveRequest)
    {
        // Respond with current patch dump
        pendingMidi_.addEvent(
            DW8000SysEx::makeDataDump(engine_.getPatch(), midiChannel_), 0);
    }
    else if (result.command == DW8000SysEx::Command::WriteRequest)
    {
        // Hardware sent: save current patch to program slot
        int slot = result.programNum & 0x3F;  // 0–63
        writeCurrentToPreset(slot);
        pendingMidi_.addEvent(DW8000SysEx::makeWriteComplete(midiChannel_), 0);
    }
}

// ─── Program / Preset bank ────────────────────────────────────────────────
void DW8000Processor::setCurrentProgram(int index)
{
    if (index < 0 || index >= NUM_PRESETS) return;
    currentPreset_ = index;
    loadPreset(index);
}

const juce::String DW8000Processor::getProgramName(int index)
{
    if (index < 0 || index >= NUM_PRESETS) return {};
    return presetNames_[index];
}

void DW8000Processor::changeProgramName(int index, const juce::String& name)
{
    if (index < 0 || index >= NUM_PRESETS) return;
    presetNames_[index] = name.isEmpty() ? slotToDisplayName(index) : name;
}

void DW8000Processor::loadPreset(int index)
{
    if (index < 0 || index >= NUM_PRESETS) return;
    currentPreset_ = index;
    loadPatch(presetBank_[index]);
}

void DW8000Processor::writeCurrentToPreset(int index)
{
    if (index < 0 || index >= NUM_PRESETS) return;
    presetBank_[index] = engine_.getPatch();
}

bool DW8000Processor::loadBankFromFile(const juce::File& f)
{
    juce::MemoryBlock mb;
    if (!f.loadFileAsData(mb)) return false;

    const auto* raw = static_cast<const uint8_t*>(mb.getData());
    const int   sz  = (int)mb.getSize();

    // Hardware format: 64 × 66 bytes (2-byte MIDI PC + 57-byte DataDump + 7-byte WriteReq)
    if (sz == NUM_PRESETS * 66)
    {
        if (!parseBankFrom66(raw, sz, presetBank_)) return false;
        loadPreset(currentPreset_);
        return true;
    }

    // Raw DataDump format: 64 × 57 bytes
    if (sz != NUM_PRESETS * 57) return false;

    for (int i = 0; i < NUM_PRESETS; ++i)
    {
        const uint8_t* msg = raw + i * 57;
        if (!DW8000SysEx::isValidDW8000Dump(msg, 57)) return false;
        presetBank_[i] = DW8000Patch::fromSysExBytes(msg + 5);
    }

    // Reload current slot so engine stays in sync
    loadPreset(currentPreset_);
    return true;
}

bool DW8000Processor::saveBankToFile(const juce::File& f) const
{
    juce::MemoryBlock mb;
    mb.setSize(64 * 57, true);
    auto* out = static_cast<uint8_t*>(mb.getData());

    for (int i = 0; i < NUM_PRESETS; ++i)
    {
        auto msg = DW8000SysEx::makeDataDump(presetBank_[i], midiChannel_);
        jassert(msg.getRawDataSize() == 57);
        std::memcpy(out + i * 57, msg.getRawData(), 57);
    }

    return f.replaceWithData(mb.getData(), mb.getSize());
}

// ─── Patch I/O ────────────────────────────────────────────────────────────
DW8000Patch DW8000Processor::getCurrentPatch() const
{
    return engine_.getPatch();
}

void DW8000Processor::loadPatch(const DW8000Patch& p)
{
    // Write every parameter into APVTS.
    // The audio thread reads these atomics each block via syncParamsToEngine(),
    // which pushes them to the engine — so no direct engine_.setPatch() needed
    // here. Calling it from whichever thread invoked loadPatch would race with
    // processBlock on the audio thread.
    auto bytes = p.toSysExBytes();
    for (int i = 0; i < NUM_PARAMS; ++i)
    {
        auto* param = dynamic_cast<juce::AudioParameterInt*>(
            apvts.getParameter(kParams[i].id));
        if (param) *param = (int)bytes[i];
    }
}

// ─── State persistence ────────────────────────────────────────────────────
void DW8000Processor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // Embed the 64-preset bank as a child element
    auto* bankEl = xml->createNewChildElement("PresetBank");
    bankEl->setAttribute("currentPreset", currentPreset_);

    for (int i = 0; i < NUM_PRESETS; ++i)
    {
        auto* slotEl = bankEl->createNewChildElement("Preset");
        slotEl->setAttribute("index", i);
        slotEl->setAttribute("name",  presetNames_[i]);

        auto bytes = presetBank_[i].toSysExBytes();
        juce::String hex;
        for (auto b : bytes)
        {
            hex += juce::String::toHexString((int)b).paddedLeft('0', 2);
            hex += " ";
        }
        slotEl->setAttribute("data", hex.trim());
    }

    copyXmlToBinary(*xml, dest);
}

void DW8000Processor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (!xmlState || !xmlState->hasTagName(apvts.state.getType()))
        return;

    apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

    // Restore preset bank
    if (auto* bankEl = xmlState->getChildByName("PresetBank"))
    {
        currentPreset_ = bankEl->getIntAttribute("currentPreset", 0);

        for (auto* slotEl : bankEl->getChildIterator())
        {
            int idx = slotEl->getIntAttribute("index", -1);
            if (idx < 0 || idx >= NUM_PRESETS) continue;

            presetNames_[idx] = slotEl->getStringAttribute("name", slotToDisplayName(idx));

            juce::String hex = slotEl->getStringAttribute("data");
            auto tokens = juce::StringArray::fromTokens(hex, " ", "");
            if (tokens.size() == 51)
            {
                std::array<uint8_t, 51> bytes{};
                for (int b = 0; b < 51; ++b)
                    bytes[b] = (uint8_t)tokens[b].getHexValue32();
                presetBank_[idx] = DW8000Patch::fromSysExBytes(bytes.data());
            }
        }
    }
    else
    {
        // No preset bank in saved state (old session / first run) → load ROM
        loadFactoryBank();
    }

    // Ensure APVTS and engine reflect the current preset
    loadPreset(currentPreset_);
}

// ─── Bus layout ───────────────────────────────────────────────────────────
bool DW8000Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo()
        || out == juce::AudioChannelSet::mono();
}

juce::AudioProcessorEditor* DW8000Processor::createEditor()
{
    return new DW8000Editor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DW8000Processor();
}
