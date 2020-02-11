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
#include "SynthOscillatorBase.h"

dsp::FFT *SynthOscillatorBase::forwardFFT;
dsp::FFT *SynthOscillatorBase::inverseFFT;

void SynthOscillatorBase::Initialize()
{
    forwardFFT = new dsp::FFT(fftOrder);
    inverseFFT = new dsp::FFT(fftOrder);

    zeromem(fftWave, sizeof(fftWave));
    for (int i = 0; i < fftSize; i++)
    {
        float phi = float(i) / fftSize;

        sineTable[i] = std::sin(2.0f * float_Pi * phi);

        fftWave[SynthWaveform::kTriangle][i] = 2.0f * (0.5f - std::fabs(phi - 0.5f)) - 1.0f;
        fftWave[SynthWaveform::kSquare][i] = (phi <= 0.5f) ? 1.0f : -1.0f;
        fftWave[SynthWaveform::kSawtooth][i] = 2.0f * phi - 1.0f;
    }

    forwardFFT->performRealOnlyForwardTransform(fftWave[SynthWaveform::kTriangle]);
    forwardFFT->performRealOnlyForwardTransform(fftWave[SynthWaveform::kSquare]);
    forwardFFT->performRealOnlyForwardTransform(fftWave[SynthWaveform::kSawtooth]);
}

void SynthOscillatorBase::Cleanup()
{
    delete forwardFFT;
    delete inverseFFT;
}

float SynthOscillatorBase::sineTable[fftSize];
SynthOscillatorBase::FFTbuf SynthOscillatorBase::fftWave[kWaveTableCount];
