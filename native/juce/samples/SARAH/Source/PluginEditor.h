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
#include "PluginProcessor.h"

class FloatParamSlider : public Slider
{
private:
    float *pParameter;
    float scaleFactor;

public:
    FloatParamSlider(float sf = 1.0f) : Slider(), pParameter(0), scaleFactor(sf) {}

    // initial setup
    void setPointer(float* pf) { pParameter = pf; notify(); }
    void setScale(float sf) { scaleFactor = sf; }

    // gets called whenever slider value is changed by user
    virtual void valueChanged() { if (pParameter) *pParameter = (float)getValue() / scaleFactor; }

    // call this when parameter value changes, to update slider
    void notify() { if (pParameter) setValue(*pParameter * scaleFactor); }
};

class IntParamSlider : public Slider
{
private:
    int* pParameter;

public:
    IntParamSlider() : Slider(), pParameter(0) {}

    void setPointer(int* pi) { pParameter = pi; notify(); }

    virtual void valueChanged() { if (pParameter) *pParameter = (int)getValue(); }

    void notify() { if (pParameter) setValue(*pParameter); }
};

class WaveformComboBox : public ComboBox
{
private:
    SynthWaveform *pWaveform;

public:
    WaveformComboBox() : ComboBox(), pWaveform(0) {}

    void setPointer(SynthWaveform* pWf) { pWaveform = pWf; notify(); }

    virtual void valueChanged(Value&) { if (pWaveform) pWaveform->fromComboBox(*this); }

    void notify() { if (pWaveform) pWaveform->toComboBox(*this); }
};

class MyLookAndFeel : public LookAndFeel_V4
{
public:
    void drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle,
        Slider& slider) override;
};


class SARAHAudioProcessorEditor
    : public AudioProcessorEditor
    , public ChangeListener
    , public ComboBox::Listener
    , public Slider::Listener
{
public:
    SARAHAudioProcessorEditor (SARAHAudioProcessor&);
    ~SARAHAudioProcessorEditor();

    void paint (Graphics&) override;
    void resized() override;

    virtual void changeListenerCallback(ChangeBroadcaster*) override;
    void comboBoxChanged(ComboBox*) override;
    void sliderValueChanged(Slider*) override;

private:
    // some empirical constants used in layout
    static const int windowWidth = 870;
    static const int windowHeight = 450;

    void PaintSarahLogo(Graphics& g);

    Image backgroundImage;
    MyLookAndFeel myLookAndFeel;
    SARAHAudioProcessor& processor;

    Colour backgroundColour;
    bool useBackgroundImage, showGroupBoxes, showLabels, showLogo, showControls;

    GroupComponent gOsc1, gPeg1, gOsc2, gPeg2, gFlt1, gFeg1, gFlt2, gFeg2, gPlfo, gHlfo, gAeg, gMaster;
    WaveformComboBox cbOsc1, cbOsc2, cbPlfo, cbHlfo;
    Label lblOsc1Pitch, lblOsc1Detune, lblOsc1PitchEgAttack, lblOsc1PitchEgSustain, lblOsc1PitchEgRelease;
    Label lblOsc2Pitch, lblOsc2Detune, lblOsc2PitchEgAttack, lblOsc2PitchEgSustain, lblOsc2PitchEgRelease;
    IntParamSlider slOsc1Pitch, slOsc2Pitch;
    FloatParamSlider slOsc1Detune, slOsc1PitchEgAttack, slOsc1PitchEgSustain, slOsc1PitchEgRelease;
    FloatParamSlider slOsc2Detune, slOsc2PitchEgAttack, slOsc2PitchEgSustain, slOsc2PitchEgRelease;
    Label lblFlt1Cutoff, lblFlt1Q, lblFlt1EnvAmt;
    Label lblFlt1EgAttack, lblFlt1EgDecay, lblFlt1EgSustain, lblFlt1EgRelease;
    Label lblFlt2Cutoff, lblFlt2Q, lblFlt2EnvAmt;
    Label lblFlt2EgAttack, lblFlt2EgDecay, lblFlt2EgSustain, lblFlt2EgRelease;
    FloatParamSlider slFlt1Cutoff, slFlt1Q, slFlt1EnvAmt;
    FloatParamSlider slFlt1EgAttack, slFlt1EgDecay, slFlt1EgSustain, slFlt1EgRelease;
    FloatParamSlider slFlt2Cutoff, slFlt2Q, slFlt2EnvAmt;
    FloatParamSlider slFlt2EgAttack, slFlt2EgDecay, slFlt2EgSustain, slFlt2EgRelease;

    Label lblPitchLfoFreq, lblPitchLfoAmount, lblFilterLfoFreq, lblFilterLfoAmount;
    Label lblMasterVol, lblOscBal, lblPitchBendUp, lblPitchBendDown;
    Label lblAmpEgAttack, lblAmpEgDecay, lblAmpEgSustain, lblAmpEgRelease;
    FloatParamSlider slPitchLfoFreq, slPitchLfoAmount, slFilterLfoFreq, slFilterLfoAmount;
    FloatParamSlider slMasterVol, slOscBal;
    IntParamSlider slPitchBendUp, slPitchBendDown;
    FloatParamSlider slAmpEgAttack, slAmpEgDecay, slAmpEgSustain, slAmpEgRelease;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SARAHAudioProcessorEditor)
};
