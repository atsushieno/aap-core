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
#include "SynthLFO.h"
#include <cmath>

// Square/Triangle/Sawtooth waves are computed directly; sine requires
// table lookup with linear interpolation.
float SynthLFO::getSineSample()
{
    // compute index of earlier sample, and fraction of way to next one
    float fsi = phase * fftSize;
    int sampleIndex = int(fsi);
    float frac = fsi - sampleIndex;

    // get earlier sample s0, and later one s1
    float s0 = sineTable[sampleIndex];
    if (++sampleIndex >= fftSize) sampleIndex = 0;
    float s1 = sineTable[sampleIndex];

    // linear interpolation
    return (1.0f - frac) * s0 + frac * s1;
}

float SynthLFO::getSample()
{
    float sample = 0.0f;

    switch (waveform.index)
    {
    case SynthWaveform::kSine:
        sample = getSineSample();
        break;
    case SynthWaveform::kSquare:
        sample = (phase <= 0.5f) ? 1.0f : -1.0f;
        break;
    case SynthWaveform::kTriangle:
        sample = 2.0f * (0.5f - std::fabs(phase - 0.5f)) - 1.0f;
        break;
    case SynthWaveform::kSawtooth:
        sample = 2.0f * phase - 1.0f;
        break;
    }

    phase += phaseDelta;
    while (phase > 1.0) phase -= 1.0;

    return sample;
}
