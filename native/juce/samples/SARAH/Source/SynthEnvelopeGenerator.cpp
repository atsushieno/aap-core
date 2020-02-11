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
#include "SynthEnvelopeGenerator.h"

SynthEnvelopeGenerator::SynthEnvelopeGenerator()
    : sampleRateHz(44100)
    , segment(EG_Segment::idle)
    , attackSeconds(0.01)
    , decaySeconds(0.1)
    , releaseSeconds(0.5)
    , sustainLevel(0.5)
{
    interpolator.setValue(0.0);
    interpolator.reset(sampleRateHz, 0.0);
}

void SynthEnvelopeGenerator::start (double _sampleRateHz)
{
    sampleRateHz = _sampleRateHz;

    if (segment == EG_Segment::idle)
    {
        // start new attack segment from zero
        interpolator.setValue(0.0);
        interpolator.reset(sampleRateHz, attackSeconds);
    }
    else
    {
        // note is still playing but has been retriggered or stolen
        // start new attack from where we are
        double currentValue = interpolator.getNextValue();
        interpolator.setValue(currentValue);
        interpolator.reset(sampleRateHz, attackSeconds * (1.0 - currentValue));
    }

    segment = EG_Segment::attack;
    interpolator.setValue(1.0);
}

void SynthEnvelopeGenerator::release()
{
    segment = EG_Segment::release;
    interpolator.setValue(interpolator.getNextValue());
    interpolator.reset(sampleRateHz, releaseSeconds);
    interpolator.setValue(0.0);
}

float SynthEnvelopeGenerator::getSample()
{
    if (segment == EG_Segment::sustain) return float(sustainLevel);

    if (interpolator.isSmoothing()) return float(interpolator.getNextValue());

    if (segment == EG_Segment::attack)    // end of attack segment
    {
        if (decaySeconds > 0.0)
        {
            // there is a decay segment
            segment = EG_Segment::decay;
            interpolator.reset(sampleRateHz, decaySeconds);
            interpolator.setValue(sustainLevel);
            return 1.0;
        }
        else
        {
            // no decay segment; go straight to sustain
            segment = EG_Segment::sustain;
            return float(sustainLevel);
        }
    }
    else if (segment == EG_Segment::decay)    // end of decay segment
    {
        segment = EG_Segment::sustain;
        return float(sustainLevel);
    }
    else if (segment == EG_Segment::release)    // end of release
    {
        segment = EG_Segment::idle;
    }

    // after end of release segment
    return 0.0f;
}
