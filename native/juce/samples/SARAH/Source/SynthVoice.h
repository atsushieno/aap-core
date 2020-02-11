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
#include "SynthSound.h"
#include "SynthOscillator.h"
#include "SynthEnvelopeGenerator.h"
#include "SynthLFO.h"

class SynthVoice : public SynthesiserVoice
{
private:
    // encapsulated objects which generate/modify sound
    SynthOscillator osc1, osc2;
    SynthEnvelopeGenerator flt1EG, flt2EG;
    SynthEnvelopeGenerator ampEG;
    SynthEnvelopeGenerator pitch1EG, pitch2EG;
    SynthLFO pitchLFO, filterLFO;

    // current sound parameters
    SynthParameters* pParams;

    // voice properties remembered from startNote() call
    float noteVelocity;

    // dynamic properties of this voice
    float pitchBend;    // converted to range [-1.0, +1.0]
    LinearSmoothedValue<float> osc1Level, osc2Level;
    bool tailOff;
    int noteSampleCounter;

public:
    SynthVoice();

    bool canPlaySound(SynthesiserSound* sound) override
    { return dynamic_cast<SynthSound*> (sound) != nullptr; }

    void soundParameterChanged();

    void startNote(int midiNoteNumber, float velocity, SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newValue) override;
    void controllerMoved(int controllerNumber, int newValue) override;

    void renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples) override;

private:
    void setPitchBend(int pitchWheelPosition);
    float pitchBendCents();
    void setup(bool pitchBendOnly);
    void pitchWobble(double cents1, double cents2);
};
