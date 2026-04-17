#include "DW8000Arpeggiator.h"

void DW8000Arpeggiator::prepare(float sampleRate)
{
    sampleRate_ = sampleRate;
}

void DW8000Arpeggiator::setSpeed(float speed)
{
    // speed 0.0–1.0 → ~400 BPM down to ~30 BPM (as 8th-note steps)
    float bpm = 30.0f + speed * 370.0f;
    samplesPerStep_ = sampleRate_ * 60.0f / (bpm * 2.0f);  // 8th notes

    // Adjust for clock param divisions
    switch (clockParam)
    {
        case 1: case 4: samplesPerStep_ *= 0.5f; break;  // 32nd
        case 2: case 5:                          break;  // 16th (default)
        case 3: case 6: samplesPerStep_ *= 2.0f; break;  // 8th
    }

    // MIDI clock division
    switch (clockParam)
    {
        case 4: midiDivision_ = 3;  break;  // 32nd
        case 5: midiDivision_ = 6;  break;  // 16th
        case 6: midiDivision_ = 12; break;  // 8th
        default: midiDivision_ = 6; break;
    }
}

void DW8000Arpeggiator::noteOn(int note)
{
    if (!latch)
    {
        if (std::find(heldNotes_.begin(), heldNotes_.end(), note) == heldNotes_.end())
            heldNotes_.push_back(note);
    }
    else
    {
        // Latch mode: add note to sequence (up to 64)
        if (latchNotes_.size() < 64)
            latchNotes_.push_back(note);
    }
    buildSequence();
    stepIndex_ = 0;
    goingUp_   = true;
}

void DW8000Arpeggiator::noteOff(int note)
{
    if (!latch)
    {
        heldNotes_.erase(std::remove(heldNotes_.begin(), heldNotes_.end(), note),
                         heldNotes_.end());
        buildSequence();
        if (sequence_.empty())
        {
            currentNote_ = -1;
            stepIndex_   = 0;
        }
    }
    // In latch mode, note-offs are ignored
}

void DW8000Arpeggiator::allNotesOff()
{
    heldNotes_.clear();
    latchNotes_.clear();
    sequence_.clear();
    currentNote_ = -1;
    stepIndex_   = 0;
    midiClockCount_ = 0;
}

void DW8000Arpeggiator::buildSequence()
{
    const auto& src = latch ? latchNotes_ : heldNotes_;
    sequence_ = src;

    if (mode == Mode::UpDown)
        std::sort(sequence_.begin(), sequence_.end());

    // Range expansion
    if (range != Range::One && !sequence_.empty())
    {
        std::vector<int> expanded = sequence_;
        int octaves = (range == Range::Two) ? 1 : 3;
        for (int oct = 1; oct <= octaves; ++oct)
            for (int n : sequence_)
                expanded.push_back(n + oct * 12);
        sequence_ = expanded;
        if (mode == Mode::UpDown)
            std::sort(sequence_.begin(), sequence_.end());
    }
}

int DW8000Arpeggiator::nextNote()
{
    if (sequence_.empty()) return -1;

    int n = sequence_.size();
    int note = sequence_[stepIndex_ % n];

    if (mode == Mode::UpDown)
    {
        if (n == 1)
        {
            stepIndex_ = 0;
        }
        else if (goingUp_)
        {
            ++stepIndex_;
            if (stepIndex_ >= n - 1) goingUp_ = false;
        }
        else
        {
            --stepIndex_;
            if (stepIndex_ <= 0) goingUp_ = true;
        }
    }
    else  // Assignable
    {
        stepIndex_ = (stepIndex_ + 1) % n;
    }

    return note;
}

void DW8000Arpeggiator::onMidiClock()
{
    if (clockParam < 4) return;  // internal clock, ignore
    ++midiClockCount_;
    if (midiClockCount_ >= midiDivision_)
    {
        midiClockCount_ = 0;
        currentNote_ = nextNote();
    }
}

bool DW8000Arpeggiator::processSample(int& activeNote, bool /*midiClockTick*/)
{
    if (!enabled || sequence_.empty())
    {
        activeNote = -1;
        return false;
    }

    if (clockParam >= 4)  // MIDI clock mode — updated via onMidiClock()
    {
        activeNote = currentNote_;
        return false;
    }

    // Internal clock
    sampleCount_ += 1.0f;
    if (sampleCount_ >= samplesPerStep_)
    {
        sampleCount_ -= samplesPerStep_;
        currentNote_ = nextNote();
        activeNote   = currentNote_;
        return true;  // new step fired
    }

    activeNote = currentNote_;
    return false;
}
