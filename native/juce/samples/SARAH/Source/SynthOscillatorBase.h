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
#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include "SynthWaveform.h"

class SynthOscillatorBase
{
public:
    SynthOscillatorBase() : phase(0.0f), phaseDelta(0.0f) {}

    // Unfortunately, FFT objects can't be allocated/initialized statically, so we need these:
    static void Initialize();    // call once when plugin is created
    static void Cleanup();        // call once when plugin is destroyed

    void setWaveform(SynthWaveform wf) { waveform = wf; }
    void setFrequency(float cyclesPerSample) { phaseDelta = cyclesPerSample; }
    float getSample() { return 0.0; }    // always override this

protected:
    SynthWaveform waveform;
    float phase;            // [0.0, 1.0]
    float phaseDelta;       // cycles per sample (fraction)

protected:
    static const int fftOrder = 10;
    static const int fftSize = 1 << fftOrder;

    static float sineTable[fftSize];

    static dsp::FFT *forwardFFT;
    static dsp::FFT *inverseFFT;
    typedef float FFTbuf[2 * fftSize];

    static const int kWaveTableCount = SynthWaveform::kNumberOfWaveformsRequiringFFT;
    static FFTbuf fftWave[kWaveTableCount];

    static float* getFourierTable(SynthWaveform wf) { return fftWave[wf.index]; }
};
