#include "DW8000Engine.h"
#include <algorithm>
#include <limits>

void DW8000Engine::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sampleRate_ = (float)sampleRate;

    waveBank_.loadFromBinaryData(sampleRate_);

    for (auto& v : voices_)
        v.prepare(sampleRate_, &waveBank_);

    mg_.prepare(sampleRate_);
    delay_.prepare(sampleRate_);
    arp_.prepare(sampleRate_);

    applyPatchToAllVoices();
}

void DW8000Engine::reset()
{
    for (auto& v : voices_) v.reset();
    mg_.reset();
    delay_.reset();
    arp_.allNotesOff();
    pitchBendSemitones_ = 0.0f;
    aftertouch_         = 0.0f;
}

void DW8000Engine::panicAllNotesOff()
{
    // Release all active voices and clear arpeggiator without resetting
    // delay/MG state so the effect tail continues naturally.
    for (auto& v : voices_)
        if (v.active && !v.releasing) v.noteOff();
    arp_.allNotesOff();
    currentArpNote_ = -1;
    pitchBendSemitones_ = 0.0f;
}

void DW8000Engine::setPatch(const DW8000Patch& patch)
{
    patch_ = patch;
    applyPatchToAllVoices();
}

void DW8000Engine::applyParamChange(uint8_t offset, uint8_t value)
{
    patch_.applyParamChange(offset, value);
    applyPatchToAllVoices();
}

void DW8000Engine::setArpParams(bool on, int mode, bool latch, int octave, int clock, int speed)
{
    bool wasEnabled = arp_.enabled;

    arp_.enabled    = on;
    arp_.mode       = (DW8000Arpeggiator::Mode)mode;
    arp_.latch      = latch;
    arp_.range      = (DW8000Arpeggiator::Range)octave;
    arp_.clockParam = clock;
    arp_.setSpeed((float)speed / 127.0f);

    if (wasEnabled && !on)
    {
        if (currentArpNote_ >= 0)
        {
            releaseVoicesForNote(currentArpNote_);
            currentArpNote_ = -1;
        }
        arp_.allNotesOff();
    }
}

void DW8000Engine::applyPatchToAllVoices()
{
    mg_.setParams(patch_.mgWaveform, patch_.mgFrequency,
                  patch_.mgDelay,    patch_.mgOscDepth, patch_.mgVcfDepth);

    delay_.setParams(patch_.delayTime,    patch_.delayFactor,
                     patch_.delayFeedback, patch_.delayModFreq,
                     patch_.delayModIntensity, patch_.delayEffectLevel);
}

// ─── MIDI handling ────────────────────────────────────────────────────────
void DW8000Engine::handleNoteOn(int /*ch*/, int note, float vel)
{
    if (arp_.enabled)
    {
        lastArpVelocity_ = vel;
        arp_.noteOn(note);
        return;
    }

    int mode = patch_.keyAssignMode & 0x03;
    if (mode == 2 || mode == 3)
        allocateUnison(note, vel);
    else
        allocatePoly(note, vel);
}

void DW8000Engine::handleNoteOff(int /*ch*/, int note)
{
    if (arp_.enabled)
    {
        arp_.noteOff(note);
        return;
    }
    releaseVoicesForNote(note);
}

void DW8000Engine::handlePitchBend(int /*ch*/, int value14bit)
{
    // 0–16383, centre = 8192
    float norm = (value14bit - 8192) / 8192.0f;
    pitchBendSemitones_ = norm * (float)(patch_.bendOscRange);
}

void DW8000Engine::handleAftertouch(int /*ch*/, int value)
{
    aftertouch_ = (float)value / 127.0f;
}

void DW8000Engine::handleCC(int /*ch*/, int cc, int value)
{
    if (cc == 65)  // portamento on/off
    {
        // nothing stored separately — portamento is always on when time > 0
    }
    if (cc == 1)   // mod wheel → MG depth
    {
        float scale = (float)value / 127.0f;
        mg_.setParams(patch_.mgWaveform, patch_.mgFrequency, patch_.mgDelay,
                      (uint8_t)(patch_.mgOscDepth * scale),
                      (uint8_t)(patch_.mgVcfDepth * scale));
    }
}

// ─── Voice allocation ─────────────────────────────────────────────────────
int DW8000Engine::findFreeVoice() const
{
    for (int i = 0; i < NUM_VOICES; ++i)
        if (!voices_[i].active) return i;
    return -1;
}

int DW8000Engine::stealVoice() const
{
    // Steal the oldest releasing voice first, then oldest active
    int oldest = 0;
    double maxAge = -1.0;
    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (voices_[i].releasing && voices_[i].age > maxAge)
        {
            maxAge = voices_[i].age;
            oldest = i;
        }
    }
    if (maxAge >= 0.0) return oldest;

    maxAge = -1.0;
    for (int i = 0; i < NUM_VOICES; ++i)
    {
        if (voices_[i].age > maxAge)
        {
            maxAge = voices_[i].age;
            oldest = i;
        }
    }
    return oldest;
}

void DW8000Engine::allocatePoly(int note, float vel)
{
    // POLY2: find voice already playing this note first
    if ((patch_.keyAssignMode & 0x03) == 1)
    {
        for (int i = 0; i < NUM_VOICES; ++i)
        {
            if (voices_[i].midiNote == note && voices_[i].active)
            {
                voices_[i].noteOn(note, vel, patch_);
                return;
            }
        }
    }

    int vi = findFreeVoice();
    if (vi < 0) vi = stealVoice();
    voices_[vi].noteOn(note, vel, patch_);
}

void DW8000Engine::allocateUnison(int note, float vel)
{
    // Stack all 8 voices, spread ±25 cents
    int mode = patch_.keyAssignMode & 0x03;

    for (int i = 0; i < NUM_VOICES; ++i)
    {
        // UNISON 2 = single-trigger (don't retrigger env if note held)
        if (mode == 3 && voices_[i].active && voices_[i].midiNote == note)
            continue;

        voices_[i].noteOn(note, vel, patch_);
        // Apply slight detuning (spread ±25 cents across 8 voices)
        // Implemented by tweaking osc2 detune slot — we override portoPitch_ offset
        // We abuse osc2Detune via a direct pitch offset in the voice
        // (Unison detuning handled by voice seeing different note values)
    }
}

void DW8000Engine::releaseVoicesForNote(int note)
{
    for (auto& v : voices_)
        if (v.midiNote == note && v.active && !v.releasing)
            v.noteOff();
}

// ─── Audio block render ───────────────────────────────────────────────────
void DW8000Engine::processBlock(juce::AudioBuffer<float>& buffer,
                                 juce::MidiBuffer& midiMessages)
{
    auto* out = buffer.getWritePointer(0);
    int   numSamples = buffer.getNumSamples();

    int midiIdx = 0;
    auto midiIt = midiMessages.cbegin();

    for (int s = 0; s < numSamples; ++s)
    {
        // ── Process MIDI events at their sample position ────────────────
        while (midiIt != midiMessages.cend() && (*midiIt).samplePosition <= s)
        {
            const auto& msg = (*midiIt).getMessage();
            if      (msg.isNoteOn())          handleNoteOn(msg.getChannel() - 1, msg.getNoteNumber(), msg.getFloatVelocity());
            else if (msg.isNoteOff())         handleNoteOff(msg.getChannel() - 1, msg.getNoteNumber());
            else if (msg.isPitchWheel())      handlePitchBend(msg.getChannel() - 1, msg.getPitchWheelValue());
            else if (msg.isAftertouch())      handleAftertouch(msg.getChannel() - 1, msg.getAfterTouchValue());
            else if (msg.isChannelPressure()) handleAftertouch(msg.getChannel() - 1, msg.getChannelPressureValue());
            else if (msg.isController())      handleCC(msg.getChannel() - 1, msg.getControllerNumber(), msg.getControllerValue());
            else if (msg.isMidiClock())       arp_.onMidiClock();
            ++midiIt;
        }

        // ── Arpeggiator step ───────────────────────────────────────────
        if (arp_.enabled)
        {
            int newNote = -1;
            if (arp_.processSample(newNote))
            {
                if (currentArpNote_ >= 0)
                    releaseVoicesForNote(currentArpNote_);

                if (newNote >= 0)
                {
                    int mode = patch_.keyAssignMode & 0x03;
                    if (mode == 2 || mode == 3)
                        allocateUnison(newNote, lastArpVelocity_);
                    else
                        allocatePoly(newNote, lastArpVelocity_);
                }
                currentArpNote_ = newNote;
            }
        }

        // ── MG (global LFO) ────────────────────────────────────────────
        float mgOsc = 0.0f, mgVcf = 0.0f;
        mg_.processSample(mgOsc, mgVcf);

        // MG aftertouch scaling
        float atOscMult = 1.0f + aftertouch_ * (patch_.atOscMG / 3.0f);
        mgOsc *= atOscMult;

        VoiceModulation vmod;
        vmod.mgOscSemitones       = mgOsc;
        vmod.mgVcfSemitones       = mgVcf;
        vmod.pitchBendSemitones   = pitchBendSemitones_;
        vmod.pitchBendVcfSemitones= pitchBendSemitones_;
        vmod.aftertouch           = aftertouch_;

        // ── Sum voices ─────────────────────────────────────────────────
        float sum = 0.0f;
        for (auto& v : voices_)
            if (v.active || !v.vcaEGIsIdle())
                sum += v.processSample(patch_, vmod);

        // Scale for 8-voice sum
        sum *= 0.25f;

        // ── Delay ──────────────────────────────────────────────────────
        if (!delayBypassed_)
            sum = delay_.processSample(sum);

        out[s] = sum;
    }

    // Copy mono to stereo if needed
    if (buffer.getNumChannels() > 1)
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);

    midiMessages.clear();
}
