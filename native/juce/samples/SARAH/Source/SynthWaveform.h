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

class SynthWaveform
{
private:
    enum WaveformTypeIndex {
        kSine = -1,
        kTriangle, kSquare, kSawtooth,  // valid for use as array indices

        kNumberOfWaveformsRequiringFFT, // waveform-types count, excluding sine
        kNumberOfWaveformTypes          // total waveform-types count
    } index;
    
    friend class SynthOscillatorBase;
    friend class SynthLFO;
    friend class SynthOscillator;

public:
    // default constructor
    SynthWaveform() : index(kSine) {}

    // set to default state after construction
    void setToDefault() { index = kSine; }

    // sine is special because we don't use array indices
    bool isSine() { return index == kSine; }

    // serialize: get human-readable name of this waveform
    String name();

    // deserialize: set index based on given name
    void setFromName(String wfName);

    // convenience funtions to allow selecting SynthWaveform from a juce::comboBox
    static void setupComboBox(ComboBox& cb);
    void fromComboBox(ComboBox& cb);
    void toComboBox(ComboBox& cb);

private:
    // waveform names: ordered list of string literals
    static const char* const wfNames[];
};
