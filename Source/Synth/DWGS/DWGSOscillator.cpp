#include "DWGSOscillator.h"

void DWGSOscillator::prepare(float sampleRate, const DWGSWavetableBank* bank)
{
    sampleRate_ = sampleRate;
    bank_       = bank;
    reset();
}

void DWGSOscillator::reset()
{
    phase_    = 0.0;
    phaseInc_ = 0.0;
}

void DWGSOscillator::setWaveform(int index)
{
    waveform_ = std::clamp(index, 0, DWGSWavetableBank::NUM_WAVEFORMS - 1);
}
