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
#include "SynthOscillatorBase.h"

class SynthParameters : public SynthOscillatorBase
{
public:
    // program name
    String programName;

    // main
    struct MainParams {
        float masterLevel;
        float oscBlend;                        // [0.0, 1.0] relative osc1 level
        int pitchBendUpSemitones;
        int pitchBendDownSemitones;
    } main;
    
    // oscillators
    struct OscillatorParams {
        SynthWaveform waveform;
        int pitchOffsetSemitones;
        float detuneOffsetCents;
    } osc1, osc2;

    // filters
    struct FilterParams {
        float cutoff;                       // [0.0, 1.0]
        float Q;                            // [0.1, 10.0]
        float envAmount;                    // [0.0, 1.0]
    } filter1, filter2;
    
    // envelope generators
    struct EnvelopeParams {
        float attackTimeSeconds;
        float decayTimeSeconds;
        float sustainLevel;                // [0.0, 1.0]
        float releaseTimeSeconds;
    } ampEG, filter1EG, filter2EG, pitch1EG, pitch2EG;

    // LFOs
    struct LFOParams {
        SynthWaveform waveform;
        float freqHz;
        float amount;        // cents for pitchLFO, percent for filterLFO
    } pitchLFO, filterLFO;

public:
    // Set default values
    void setDefaultValues();

    // Save and Restore from XML
    XmlElement* getXml();
    void putXml(XmlElement* xml);
};
