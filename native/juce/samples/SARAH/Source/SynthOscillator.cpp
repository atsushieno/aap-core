/* Copyright (c) 2017-2018 Shane D. Dunne

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include "SynthOscillator.h"
#include <cmath>
#include "../JuceLibraryCode/JuceHeader.h"    // only for double_Pi constant

SynthOscillator::SynthOscillator()
    : SynthOscillatorBase()
    , noteNumber(-1)
{
    setFilterParams(1.0f, 1.0f);
}

void SynthOscillator::setFrequency(double sampleRateHz, int midiNoteNumber, double centOffset)
{
    double cyclesPerSecond = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    cyclesPerSecond *= std::pow(2.0, centOffset / 1200);
    phaseDelta = float(cyclesPerSecond / sampleRateHz);
    int maxHarmonicToRetain = int(1.0 / (2.0 * phaseDelta));
    if (maxHarmonicToRetain >= fftSize / 2) maxHarmonicToRetain = fftSize / 2;

    if (noteNumber != midiNoteNumber && waveform.isSine())
    {
        zeromem(waveTable, sizeof(waveTable));
        for (int ip = 1; ip < maxHarmonicToRetain; ip++)
        {
            int in = fftSize - ip;
            float* fftBuffer = SynthOscillatorBase::getFourierTable(waveform);
            memcpy(waveTable + ip, fftBuffer + ip, 2 * sizeof(float)); // positive frequency coefficient
            memcpy(waveTable + in, fftBuffer + in, 2 * sizeof(float)); // negative frequency coefficient
        }
        inverseFFT->performRealOnlyInverseTransform(waveTable);
    }
    noteNumber = midiNoteNumber;
}

void SynthOscillator::setFilterParams(float cutoffFraction, float qFactor)
{
    filterCutoff = cutoffFraction;
    filterQ = qFactor;
    filterModel.setup(filterCutoff * 0.5, filterQ);
}

void SynthOscillator::setFilterCutoffDelta(float fcDelta)
{
    if (waveform.isSine()) return;

    zeromem(waveTable, sizeof(waveTable));
    int maxHarmonicToRetain = int(1.0 / (2.0 * phaseDelta));
    if (maxHarmonicToRetain >= fftSize / 2) maxHarmonicToRetain = fftSize / 2;

    filterModel.setup((filterCutoff + fcDelta) * 0.5, filterQ);

    for (int ip = 1; ip < maxHarmonicToRetain; ip++)
    {
        int in = fftSize - ip;
        float* fftBuffer = SynthOscillatorBase::getFourierTable(waveform);
        memcpy(waveTable + ip, fftBuffer + ip, 2 * sizeof(float)); // positive frequency coefficient
        memcpy(waveTable + in, fftBuffer + in, 2 * sizeof(float)); // negative frequency coefficient

        // Apply our filter model to simulate resonant filter response
        float fv = float(filterModel.response(double(ip) / (fftSize / 2)));
        waveTable[ip + 0] *= fv;
        waveTable[ip + 1] *= fv;
        waveTable[in + 0] *= fv;
        waveTable[in + 1] *= fv;
    }
    inverseFFT->performRealOnlyInverseTransform(waveTable);
}

float SynthOscillator::getSample()
{
    int sampleIndex = int(phase * fftSize + 0.5f);
    if (sampleIndex >= fftSize) sampleIndex = 0;

    phase += phaseDelta;
    while (phase > 1.0) phase -= 1.0;

    if (waveform.isSine()) return sineTable[sampleIndex];
    else return waveTable[sampleIndex];
}
