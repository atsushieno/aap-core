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
#include "SynthOscillatorBase.h"

static const double doublePi = 3.1415926535897932384626433832795028841971;

template <typename Ty, typename To>
inline std::complex<Ty> addmul(const std::complex<Ty>& c, Ty v, const std::complex<To>& c1)
{
    return std::complex <Ty>(c.real() + v * c1.real(), c.imag() + v * c1.imag());
}

typedef std::complex<double> complex_t;

// This "model" of a traditional "RBJ Biquad" filter is adapted from Vinnie Falco's
// "Collection of Useful C++ Classes for Digital Signal Processing"
// See https://github.com/vinniefalco/DSPFilters
class BiquadFilterModel
{
private:
    double a0, a1, a2, b0, b1, b2;

public:
    // normalizedCutoffFrequency varies from 0 to 0.5 (Nyquist)
    // q varies from about 0.1 to maybe 10.0 or a bit more
    void setup(double normalizedCutoffFrequency, double q)
    {
        double w0 = 2 * doublePi * normalizedCutoffFrequency;
        double cs = cos(w0);
        double sn = sin(w0);
        double AL = sn / (2 * q);

        a0 = 1 + AL;
        a1 = -2 * cs;
        a2 = 1 - AL;

        b0 = (1 - cs) / 2;
        b1 = 1 - cs;
        b2 = (1 - cs) / 2;
    }

    // normalizedFrequency varies from 0 to 0.5 (Nyquist)
    double response(double normalizedFrequency)
    {
        double w = 2 * doublePi * normalizedFrequency;
        complex_t czn1 = std::polar(1., -w);
        complex_t czn2 = std::polar(1., -2 * w);
        complex_t ch(1);
        complex_t cbot(1);

        complex_t ct(b0 / a0);
        complex_t cb(1);
        ct = addmul(ct, b1 / a0, czn1);
        ct = addmul(ct, b2 / a0, czn2);
        cb = addmul(cb, a1 / a0, czn1);
        cb = addmul(cb, a2 / a0, czn2);
        ch *= ct;
        cbot *= cb;

        return sqrt(std::norm(ch / cbot));
    }
};

class SynthOscillator : public SynthOscillatorBase
{
public:
    SynthOscillator();
    
    void setFrequency(double sampleRateHz, int midiNoteNumber, double centOffset);
    float getSample();

    void setFilterParams(float cutoffFraction, float qFactor);
    void setFilterCutoffDelta(float fcDelta);

private:
    int noteNumber;
    float waveTable[2 * fftSize];

    float filterCutoff;         // a fraction [0.0, 1,0]
    float filterQ;              // Q factor, range 0.1-ish to 10-ish

    BiquadFilterModel filterModel;
};
