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
#include "PluginProcessor.h"
#include "PluginEditor.h"

// Change the look of our "rotary sliders" so they're more like traditional knobs. This code is adapted
// from the example at https://www.juce.com/doc/tutorial_look_and_feel_customisation.
void MyLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
    const float rotaryStartAngle, const float rotaryEndAngle,
    Slider& slider)
{
    ignoreUnused(slider);

    const float radius = jmin(width / 2, height / 2) - 10.0f;
    const float centreX = x + width * 0.5f;
    const float centreY = y + height * 0.5f;
    const float rx = centreX - radius;
    const float ry = centreY - radius;
    const float rw = radius * 2.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // fill
    g.setColour(Colours::steelblue);
    g.fillEllipse(rx, ry, rw, rw);
    // outline
    g.setColour(Colours::slategrey);
    g.drawEllipse(rx, ry, rw, rw, 1.0f);

    Path p;
    const float pointerLength = radius * 0.5f;
    const float pointerThickness = 2.0f;
    p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
    p.applyTransform(AffineTransform::rotation(angle).translated(centreX, centreY));

    // pointer
    g.setColour(Colours::lightblue);
    g.fillPath(p);
}

SARAHAudioProcessorEditor::SARAHAudioProcessorEditor (SARAHAudioProcessor& p)
    : AudioProcessorEditor (&p)
    , processor (p)
    , backgroundColour(0xff323e44)  // default JUCE background colour
    , useBackgroundImage(false)
    , showGroupBoxes(true)
    , showLabels(true)
    , showLogo(true)
    , showControls(true)
    , gOsc1("gOsc1", TRANS("Oscillator 1"))
    , gPeg1("gPeg1", TRANS("Pitch EG"))
    , gOsc2("gOsc2", TRANS("Oscillator 2"))
    , gPeg2("gPeg2", TRANS("Pitch EG"))
    , gFlt1("gFlt1", TRANS("Harmonic Shaping 1"))
    , gFeg1("gFeg1", TRANS("Harmonic EG"))
    , gFlt2("gFlt2", TRANS("Harmonic Shaping 2"))
    , gFeg2("gFeg2", TRANS("Harmonic EG"))
    , gPlfo("gPlfo", TRANS("Pitch LFO"))
    , gHlfo("gHlfo", TRANS("Harmonic LFO"))
    , gAeg("gAeg", TRANS("Amplitude EG"))
    , gMaster("gMaster", TRANS("Master"))
    , lblOsc1Pitch("lblOsc1Pitch", TRANS("Pitch"))
    , lblOsc1Detune("lblOsc1Detune", TRANS("Detune"))
    , lblOsc1PitchEgAttack("lblOsc1PitchEgAttack", TRANS("Attack"))
    , lblOsc1PitchEgSustain("lblOsc1PitchEgSustain", TRANS("Sustain"))
    , lblOsc1PitchEgRelease("lblOsc1PitchEgRelease", TRANS("Release"))
    , lblOsc2Pitch("lblOsc2Pitch", TRANS("Pitch"))
    , lblOsc2Detune("lblOsc2Detune", TRANS("Detune"))
    , lblOsc2PitchEgAttack("lblOsc2PitchEgAttack", TRANS("Attack"))
    , lblOsc2PitchEgSustain("lblOsc2PitchEgSustain", TRANS("Sustain"))
    , lblOsc2PitchEgRelease("lblOsc2PitchEgRelease", TRANS("Release"))
    , lblFlt1Cutoff("lblFlt1Cutoff", TRANS("Cutoff"))
    , lblFlt1Q("lblFlt1Q", TRANS("Resonance"))
    , lblFlt1EnvAmt("lblFlt1EnvAmt", TRANS("Amount"))
    , lblFlt1EgAttack("lblFlt1EgAttack", TRANS("Attack"))
    , lblFlt1EgDecay("lblFlt1EgDecay", TRANS("Decay"))
    , lblFlt1EgSustain("lblFlt1EgSustain", TRANS("Sustain"))
    , lblFlt1EgRelease("lblFlt1EgRelease", TRANS("Release"))
    , lblFlt2Cutoff("lblFlt2Cutoff", TRANS("Cutoff"))
    , lblFlt2Q("lblFltQ", TRANS("Resonance"))
    , lblFlt2EnvAmt("lblFlt2EnvAmt", TRANS("Amount"))
    , lblFlt2EgAttack("lblFlt2EgAttack", TRANS("Attack"))
    , lblFlt2EgDecay("lblFlt2EgDecay", TRANS("Decay"))
    , lblFlt2EgSustain("lblFlt2EgSustain", TRANS("Sustain"))
    , lblFlt2EgRelease("lblFlt2EgRelease", TRANS("Release"))
    , lblPitchLfoFreq("lblPitchLfoFreq", TRANS("Freq"))
    , lblPitchLfoAmount("lblPitchLfoAmount", TRANS("Amount"))
    , lblFilterLfoFreq("lblFilterLfoFreq", TRANS("Freq"))
    , lblFilterLfoAmount("lblFilterLfoAmount", TRANS("Amount"))
    , lblMasterVol("lblMasterVol", TRANS("Volume"))
    , lblOscBal("lblOscBal", TRANS("Osc.Bal"))
    , lblPitchBendUp("lblPitchBendUp", TRANS("PB.Up"))
    , lblPitchBendDown("lblPitchBendDown", TRANS("PB.Dn"))
    , lblAmpEgAttack("lblAmpEgAttack", TRANS("Attack"))
    , lblAmpEgDecay("lblAmpEgDecay", TRANS("Decay"))
    , lblAmpEgSustain("lblAmpEgSustain", TRANS("Sustain"))
    , lblAmpEgRelease ("lblAmpEgRelease", TRANS("Release"))
{
    setSize (windowWidth, windowHeight);
    p.addChangeListener(this);

    setLookAndFeel(&myLookAndFeel);

    // If you intend to take screenshots as basis for developing custom backgrounds,
    // modify the defaults for backgroundColour, useBackgroundImage, showGroupBoxes,
    // showLabels, showLogo, showControls here, e.g.
    //backgroundColour = Colour(0xff000000);  // opaque black

    backgroundImage = ImageCache::getFromMemory(BinaryData::background_png, BinaryData::background_pngSize);
    File fileOnDesktop = File::getSpecialLocation(File::SpecialLocationType::userDesktopDirectory).getChildFile("sarah.png");
    if (fileOnDesktop.existsAsFile())
    {
        // if user has provided a background image, we always use it
        backgroundImage = ImageFileFormat::loadFrom(fileOnDesktop);
        useBackgroundImage = true;
    }

    if (useBackgroundImage)
    {
        // display only controls, assuming all static material is in the background image
        showGroupBoxes = false;
        showLabels = false;
        showLogo = false;
    }

    if (showGroupBoxes)
    {
        addAndMakeVisible(gOsc1);
        addAndMakeVisible(gPeg1);
        addAndMakeVisible(gOsc2);
        addAndMakeVisible(gPeg2);
        addAndMakeVisible(gFlt1);
        addAndMakeVisible(gFeg1);
        addAndMakeVisible(gFlt2);
        addAndMakeVisible(gFeg2);
        addAndMakeVisible(gPlfo);
        addAndMakeVisible(gHlfo);
        addAndMakeVisible(gAeg);
        addAndMakeVisible(gMaster);
    }

    auto initLabel = [this](Label& label)
    {
        if (showLabels) addAndMakeVisible(label);
        label.setJustificationType(Justification::horizontallyCentred);
        label.setEditable(false, false, false);
    };

    initLabel(lblOsc1Pitch);
    initLabel(lblOsc1Detune);
    initLabel(lblOsc1PitchEgAttack);
    initLabel(lblOsc1PitchEgSustain);
    initLabel(lblOsc1PitchEgRelease);
    initLabel(lblOsc2Pitch);
    initLabel(lblOsc2Detune);
    initLabel(lblOsc2PitchEgAttack);
    initLabel(lblOsc2PitchEgSustain);
    initLabel(lblOsc2PitchEgRelease);
    initLabel(lblFlt1Cutoff);
    initLabel(lblFlt1Q);
    initLabel(lblFlt1EnvAmt);
    initLabel(lblFlt1EgAttack);
    initLabel(lblFlt1EgDecay);
    initLabel(lblFlt1EgSustain);
    initLabel(lblFlt1EgRelease);
    initLabel(lblFlt2Cutoff);
    initLabel(lblFlt2Q);
    initLabel(lblFlt2EnvAmt);
    initLabel(lblFlt2EgAttack);
    initLabel(lblFlt2EgDecay);
    initLabel(lblFlt2EgSustain);
    initLabel(lblFlt2EgRelease);
    initLabel(lblPitchLfoFreq);
    initLabel(lblPitchLfoAmount);
    initLabel(lblFilterLfoFreq);
    initLabel(lblFilterLfoAmount);
    initLabel(lblMasterVol);
    initLabel(lblOscBal);
    initLabel(lblPitchBendUp);
    initLabel(lblPitchBendDown);
    initLabel(lblAmpEgAttack);
    initLabel(lblAmpEgDecay);
    initLabel(lblAmpEgSustain);
    initLabel(lblAmpEgRelease);

    auto initCombo = [this](WaveformComboBox& combo)
    {
        if (showControls) addAndMakeVisible(combo);
        combo.setEditableText(false);
        combo.setJustificationType(Justification::centredLeft);
        combo.setTextWhenNothingSelected("");
        combo.setTextWhenNoChoicesAvailable(TRANS("(no choices)"));
        combo.addItem(TRANS("Sine"), 1);
        combo.addItem(TRANS("Triangle"), 2);
        combo.addItem(TRANS("Square"), 3);
        combo.addItem(TRANS("Sawtooth"), 4);
        combo.addListener(this);
    };

    initCombo(cbOsc1);
    initCombo(cbOsc2);
    initCombo(cbPlfo);
    initCombo(cbHlfo);

    auto initSlider = [this](Slider& slider)
    {
        if (showControls) addAndMakeVisible(slider);
        slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled(true, true, 0);
        slider.addListener(this);
    };

    initSlider(slOsc1Pitch); slOsc1Pitch.setRange(-24, 24, 1);
    initSlider(slOsc2Pitch); slOsc2Pitch.setRange(-24, 24, 1);
    initSlider(slOsc1Detune); slOsc1Detune.setRange(-50, 50, 0);
    initSlider(slOsc2Detune); slOsc2Detune.setRange(-50, 50, 0);
    initSlider(slOsc1PitchEgAttack); slOsc1PitchEgAttack.setRange(0, 10, 0);
    initSlider(slOsc1PitchEgSustain); slOsc1PitchEgSustain.setRange(-12, 12, 0);
    initSlider(slOsc1PitchEgRelease); slOsc1PitchEgRelease.setRange(0, 10, 0);
    initSlider(slOsc2PitchEgAttack); slOsc2PitchEgAttack.setRange(0, 10, 0);
    initSlider(slOsc2PitchEgSustain); slOsc2PitchEgSustain.setRange(-12, 12, 0);
    initSlider(slOsc2PitchEgRelease); slOsc2PitchEgRelease.setRange(0, 10, 0);
    initSlider(slFlt1Cutoff); slFlt1Cutoff.setRange(0, 100, 0); slFlt1Cutoff.setScale(100);
    initSlider(slFlt1Q); slFlt1Q.setRange(0.1, 6, 0);
    initSlider(slFlt1EnvAmt); slFlt1EnvAmt.setRange(0, 100, 0); slFlt1EnvAmt.setScale(100);
    initSlider(slFlt1EgAttack); slFlt1EgAttack.setRange(0, 10, 0);
    initSlider(slFlt1EgDecay); slFlt1EgDecay.setRange(0, 10, 0);
    initSlider(slFlt1EgSustain); slFlt1EgSustain.setRange(0, 100, 0); slFlt1EgSustain.setScale(100);
    initSlider(slFlt1EgRelease); slFlt1EgRelease.setRange(0, 10, 0);
    initSlider(slFlt2Cutoff); slFlt2Cutoff.setRange(0, 100, 0); slFlt2Cutoff.setScale(100);
    initSlider(slFlt2Q); slFlt2Q.setRange(0.1, 6, 0);
    initSlider(slFlt2EnvAmt); slFlt2EnvAmt.setRange(0, 100, 0); slFlt2EnvAmt.setScale(100);
    initSlider(slFlt2EgAttack); slFlt2EgAttack.setRange(0, 10, 0);
    initSlider(slFlt2EgDecay); slFlt2EgDecay.setRange(0, 10, 0);
    initSlider(slFlt2EgSustain); slFlt2EgSustain.setRange(0, 100, 0); slFlt2EgSustain.setScale(100);
    initSlider(slFlt2EgRelease); slFlt2EgRelease.setRange(0, 10, 0);
    initSlider(slPitchLfoFreq); slPitchLfoFreq.setRange(0.1, 10, 0);
    initSlider(slPitchLfoAmount); slPitchLfoAmount.setRange(0, 100, 0);
    initSlider(slFilterLfoFreq); slPitchLfoFreq.setRange(0.1, 10, 0);
    initSlider(slFilterLfoAmount); slFilterLfoAmount.setRange(0, 100, 0); slFilterLfoAmount.setScale(100);
    initSlider(slMasterVol); slMasterVol.setRange(0, 100, 0); slMasterVol.setScale(100);
    initSlider(slOscBal); slOscBal.setRange(0, 1, 0);
    initSlider(slPitchBendUp); slPitchBendUp.setRange(0, 12, 1);
    initSlider(slPitchBendDown); slPitchBendDown.setRange(0, 12, 1);
    initSlider(slAmpEgAttack); slAmpEgAttack.setRange(0, 10, 0);
    initSlider(slAmpEgDecay); slAmpEgDecay.setRange(0, 10, 0);
    initSlider(slAmpEgSustain); slAmpEgSustain.setRange(0, 100, 0); slAmpEgSustain.setScale(100);
    initSlider(slAmpEgRelease); slAmpEgRelease.setRange(0, 10, 0);

    slOsc1Pitch.setDoubleClickReturnValue(true, 0.0);
    slOsc2Pitch.setDoubleClickReturnValue(true, 0.0);
    slOsc1Detune.setDoubleClickReturnValue(true, 0.0);
    slOsc2Detune.setDoubleClickReturnValue(true, 0.0);
    slOsc1PitchEgAttack.setDoubleClickReturnValue(true, 0.0);
    slOsc1PitchEgSustain.setDoubleClickReturnValue(true, 0.0);
    slOsc1PitchEgRelease.setDoubleClickReturnValue(true, 0.0);
    slOsc2PitchEgAttack.setDoubleClickReturnValue(true, 0.0);
    slOsc2PitchEgSustain.setDoubleClickReturnValue(true, 0.0);
    slOsc2PitchEgRelease.setDoubleClickReturnValue(true, 0.0);
    slFlt1Cutoff.setDoubleClickReturnValue(true, 1.0);
    slFlt1Q.setDoubleClickReturnValue(true, 1.0);
    slFlt1EgAttack.setDoubleClickReturnValue(true, 0.0);
    slFlt1EgDecay.setDoubleClickReturnValue(true, 0.0);
    slFlt1EgSustain.setDoubleClickReturnValue(true, 1.0);
    slFlt2EgRelease.setDoubleClickReturnValue(true, 0.0);
    slFlt2Cutoff.setDoubleClickReturnValue(true, 1.0);
    slFlt2Q.setDoubleClickReturnValue(true, 1.0);
    slFlt2EgAttack.setDoubleClickReturnValue(true, 0.0);
    slFlt2EgDecay.setDoubleClickReturnValue(true, 0.0);
    slFlt2EgSustain.setDoubleClickReturnValue(true, 1.0);
    slFlt2EgRelease.setDoubleClickReturnValue(true, 0.0);
    slPitchLfoFreq.setDoubleClickReturnValue(true, 5.0);
    slPitchLfoAmount.setDoubleClickReturnValue(true, 0.0);
    slPitchLfoFreq.setDoubleClickReturnValue(true, 5.0);
    slFilterLfoAmount.setDoubleClickReturnValue(true, 0.0);
    slMasterVol.setDoubleClickReturnValue(true, 0.15);
    slOscBal.setDoubleClickReturnValue(true, 0.5);
    slPitchBendUp.setDoubleClickReturnValue(true, 2.0);
    slPitchBendDown.setDoubleClickReturnValue(true, 2.0);
    slAmpEgAttack.setDoubleClickReturnValue(true, 0.1);
    slAmpEgDecay.setDoubleClickReturnValue(true, 0.1);
    slAmpEgSustain.setDoubleClickReturnValue(true, 0.8);
    slAmpEgRelease.setDoubleClickReturnValue(true, 0.5);

    changeListenerCallback(0);  // set all control pointers
}

SARAHAudioProcessorEditor::~SARAHAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    processor.removeChangeListener(this);
}

void SARAHAudioProcessorEditor::PaintSarahLogo(Graphics& g)
{
    // basic letter structure
    static int width = 7;
    static int spacing = 2;
    static int r_indent = 2;

    // see resized() below
    const int topOffset1 = 12;
    const int largeGroupHeight = 180;

    // vertical layout
    const int logoTopOffset = 16;
    const int logoHeight = 32;
    const int logoTop = topOffset1 + largeGroupHeight + logoTopOffset;
    const int logoMid = logoTop + logoHeight / 2;
    const int logoBottom = logoTop + logoHeight;

    // horizontal layout
    static float widthScale = 10.0f;
    static float letterWidth = widthScale * width;
    static float rWidth = widthScale * (width - r_indent);
    static float letterAdvance = widthScale * (width + spacing);
    static float wordWidth = widthScale * (5 * width + 4 * spacing);
    static float logoLeft = windowWidth / 2 - 0.5f * wordWidth;

    static float lineThickness = 5.0f;
    g.setColour(Colours::steelblue);

    // Letter S
    float left = logoLeft;
    g.drawLine(left, logoTop, left + letterWidth, logoTop, lineThickness);
    g.drawLine(left, logoTop, left, logoMid, lineThickness);
    g.drawLine(left, logoMid, left + letterWidth, logoMid, lineThickness);
    g.drawLine(left + letterWidth, logoMid, left + letterWidth, logoBottom, lineThickness);
    g.drawLine(left, logoBottom, left + letterWidth, logoBottom, lineThickness);

    // Letter A
    left += letterAdvance;
    g.drawLine(left, logoTop, left + letterWidth, logoTop, lineThickness);
    g.drawLine(left, logoTop, left, logoBottom, lineThickness);
    g.drawLine(left + letterWidth, logoTop, left + letterWidth, logoBottom, lineThickness);
    g.drawLine(left, logoMid, left + letterWidth, logoMid, lineThickness);

    // Letter R
    left += letterAdvance;
    g.drawLine(left, logoTop, left + letterWidth, logoTop, lineThickness);
    g.drawLine(left, logoTop, left, logoBottom, lineThickness);
    g.drawLine(left + letterWidth, logoTop, left + letterWidth, logoMid, lineThickness);
    g.drawLine(left, logoMid, left + letterWidth, logoMid, lineThickness);
    g.drawLine(left + rWidth, logoMid, left + letterWidth, logoBottom, lineThickness);

    // Letter A
    left += letterAdvance;
    g.drawLine(left, logoTop, left + letterWidth, logoTop, lineThickness);
    g.drawLine(left, logoTop, left, logoBottom, lineThickness);
    g.drawLine(left + letterWidth, logoTop, left + letterWidth, logoBottom, lineThickness);
    g.drawLine(left, logoMid, left + letterWidth, logoMid, lineThickness);

    // Letter H
    left += letterAdvance;
    g.drawLine(left, logoTop, left, logoBottom, lineThickness);
    g.drawLine(left + letterWidth, logoTop, left + letterWidth, logoBottom, lineThickness);
    g.drawLine(left, logoMid, left + letterWidth, logoMid, lineThickness);
}

void SARAHAudioProcessorEditor::paint(Graphics& g)
{
    if (useBackgroundImage)
        g.drawImageAt(backgroundImage, 0, 0);
    else
        g.fillAll(backgroundColour);

    if (showLogo) PaintSarahLogo(g);
}

void SARAHAudioProcessorEditor::resized()
{
    const int leftOffset = 14;
    const int groupWidth = 270;
    const int cboxWidth = 100;
    const int egLeftOffset = 20;
    const int egWidth = 230;
    const int groupHorizontalGap = 15;
    const int slLeftOffset = 38;
    const int slLeftAdvance = 52;
    const int lblWidth = 90;

    const int topOffset1 = 12;
    const int largeGroupHeight = 180;
    const int smallGroupHeight = 85;
    const int middleGap = 60;
    const int topOffset2 = topOffset1 + largeGroupHeight + middleGap;
    const int egTopOffset = 80;
    const int egHeight = 84;
    const int groupVerticalGap = 10;
    const int cbTopOffset = 30;
    const int cbHeight = 24;
    const int slTopOffset = 12;
    const int slSize = 56;
    const int lblTopOffset = 62;
    const int lblHeight = 15;

    int left, top, slLeftCtr;

    // First column: OSC1
    left = leftOffset;
    top = topOffset1;
    gOsc1.setBounds(left, top, groupWidth, largeGroupHeight);
    cbOsc1.setBounds(left + egLeftOffset, top + cbTopOffset, cboxWidth, cbHeight);
    slLeftCtr = left + egLeftOffset + cboxWidth + slLeftOffset;
    slOsc1Pitch.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblOsc1Pitch.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset + 3.1 * slLeftAdvance;
    slOsc1Detune.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblOsc1Detune.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    gPeg1.setBounds(left + egLeftOffset, top + egTopOffset, egWidth, egHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slOsc1PitchEgAttack.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblOsc1PitchEgAttack.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slOsc1PitchEgSustain.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblOsc1PitchEgSustain.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slOsc1PitchEgRelease.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblOsc1PitchEgRelease.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);

    // First column: OSC2
    top = topOffset2;
    gOsc2.setBounds(left, top, groupWidth, largeGroupHeight);
    cbOsc2.setBounds(left + egLeftOffset, top + cbTopOffset, cboxWidth, cbHeight);
    slLeftCtr = left + egLeftOffset + cboxWidth + slLeftOffset;
    slOsc2Pitch.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblOsc2Pitch.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset + 3.1 * slLeftAdvance;
    slOsc2Detune.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblOsc2Detune.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    gPeg2.setBounds(left + egLeftOffset, top + egTopOffset, egWidth, egHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slOsc2PitchEgAttack.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblOsc2PitchEgAttack.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slOsc2PitchEgSustain.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblOsc2PitchEgSustain.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slOsc2PitchEgRelease.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblOsc2PitchEgRelease.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);

    // Second column: HS1
    left += groupWidth + groupHorizontalGap;
    top = topOffset1;
    gFlt1.setBounds(left, top, groupWidth, largeGroupHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slFlt1Cutoff.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFlt1Cutoff.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slFlt1Q.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFlt1Q.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slFlt1EnvAmt.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFlt1EnvAmt.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    gFeg1.setBounds(left + egLeftOffset, top + egTopOffset, egWidth, egHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slFlt1EgAttack.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt1EgAttack.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slFlt1EgDecay.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt1EgDecay.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slFlt1EgSustain.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt1EgSustain.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slFlt1EgRelease.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt1EgRelease.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);

    // Second column: HS2
    top = topOffset2;
    gFlt2.setBounds(left, top, groupWidth, largeGroupHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slFlt2Cutoff.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFlt2Cutoff.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slFlt2Q.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFlt2Q.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += 1.5 * slLeftAdvance;
    slFlt2EnvAmt.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFlt2EnvAmt.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    gFeg2.setBounds(left + egLeftOffset, top + egTopOffset, egWidth, egHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slFlt2EgAttack.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt2EgAttack.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slFlt2EgDecay.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt2EgDecay.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slFlt2EgSustain.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt2EgSustain.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slFlt2EgRelease.setBounds(slLeftCtr - slSize / 2, top + egTopOffset + slTopOffset, slSize, slSize);
    lblFlt2EgRelease.setBounds(slLeftCtr - lblWidth / 2, top + egTopOffset + lblTopOffset, lblWidth, lblHeight);

    // Third column: Pitch LFO
    left += groupWidth + groupHorizontalGap;
    top = topOffset1;
    gPlfo.setBounds(left, top, groupWidth, smallGroupHeight);
    cbPlfo.setBounds(left + egLeftOffset, top + cbTopOffset, cboxWidth, cbHeight);
    slLeftCtr = left + egLeftOffset + cboxWidth + slLeftOffset;
    slPitchLfoFreq.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblPitchLfoFreq.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset + 3.1 * slLeftAdvance;
    slPitchLfoAmount.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblPitchLfoAmount.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);

    // Third column: Harmonic LFO
    top += smallGroupHeight + groupVerticalGap;
    gHlfo.setBounds(left, top, groupWidth, smallGroupHeight);
    cbHlfo.setBounds(left + egLeftOffset, top + cbTopOffset, cboxWidth, cbHeight);
    slLeftCtr = left + egLeftOffset + cboxWidth + slLeftOffset;
    slFilterLfoFreq.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFilterLfoFreq.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset + 3.1 * slLeftAdvance;
    slFilterLfoAmount.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblFilterLfoAmount.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);

    // Third column: Amp EG
    top = topOffset2;
    gAeg.setBounds(left, top, groupWidth, smallGroupHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slAmpEgAttack.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblAmpEgAttack.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slAmpEgDecay.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblAmpEgDecay.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slAmpEgSustain.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblAmpEgSustain.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slAmpEgRelease.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblAmpEgRelease.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);

    // Third column: Master
    top += smallGroupHeight + groupVerticalGap;
    gMaster.setBounds(left, top, groupWidth, smallGroupHeight);
    slLeftCtr = left + egLeftOffset + slLeftOffset;
    slMasterVol.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblMasterVol.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slOscBal.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblOscBal.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slPitchBendUp.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblPitchBendUp.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
    slLeftCtr += slLeftAdvance;
    slPitchBendDown.setBounds(slLeftCtr - slSize / 2, top + slTopOffset, slSize, slSize);
    lblPitchBendDown.setBounds(slLeftCtr - lblWidth / 2, top + lblTopOffset, lblWidth, lblHeight);
}

void SARAHAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster*)
{
    SynthParameters* pParams = processor.getSound()->pParams;

    cbOsc1.setPointer(&pParams->osc1.waveform);
    cbOsc2.setPointer(&pParams->osc2.waveform);
    cbPlfo.setPointer(&pParams->pitchLFO.waveform);
    cbHlfo.setPointer(&pParams->filterLFO.waveform);

    slOsc1Pitch.setPointer(&pParams->osc1.pitchOffsetSemitones);
    slOsc1Detune.setPointer(&pParams->osc1.detuneOffsetCents);
    slOsc1PitchEgAttack.setPointer(&pParams->pitch1EG.attackTimeSeconds);
    slOsc1PitchEgSustain.setPointer(&pParams->pitch1EG.sustainLevel);
    slOsc1PitchEgRelease.setPointer(&pParams->pitch1EG.releaseTimeSeconds);

    slOsc2Pitch.setPointer(&pParams->osc2.pitchOffsetSemitones);
    slOsc2Detune.setPointer(&pParams->osc2.detuneOffsetCents);
    slOsc2PitchEgAttack.setPointer(&pParams->pitch2EG.attackTimeSeconds);
    slOsc2PitchEgSustain.setPointer(&pParams->pitch2EG.sustainLevel);
    slOsc2PitchEgRelease.setPointer(&pParams->pitch2EG.releaseTimeSeconds);

    slFlt1Cutoff.setPointer(&pParams->filter1.cutoff);
    slFlt1Q.setPointer(&pParams->filter1.Q);
    slFlt1EnvAmt.setPointer(&pParams->filter1.envAmount);
    slFlt1EgAttack.setPointer(&pParams->filter1EG.attackTimeSeconds);
    slFlt1EgDecay.setPointer(&pParams->filter1EG.decayTimeSeconds);
    slFlt1EgSustain.setPointer(&pParams->filter1EG.sustainLevel);
    slFlt1EgRelease.setPointer(&pParams->filter1EG.releaseTimeSeconds);

    slFlt2Cutoff.setPointer(&pParams->filter2.cutoff);
    slFlt2Q.setPointer(&pParams->filter2.Q);
    slFlt2EnvAmt.setPointer(&pParams->filter2.envAmount);
    slFlt2EgAttack.setPointer(&pParams->filter2EG.attackTimeSeconds);
    slFlt2EgDecay.setPointer(&pParams->filter2EG.decayTimeSeconds);
    slFlt2EgSustain.setPointer(&pParams->filter2EG.sustainLevel);
    slFlt2EgRelease.setPointer(&pParams->filter2EG.releaseTimeSeconds);

    slPitchLfoFreq.setPointer(&pParams->pitchLFO.freqHz);
    slPitchLfoAmount.setPointer(&pParams->pitchLFO.amount);
    slFilterLfoFreq.setPointer(&pParams->filterLFO.freqHz);
    slFilterLfoAmount.setPointer(&pParams->filterLFO.amount);
    slMasterVol.setPointer(&pParams->main.masterLevel);
    slOscBal.setPointer(&pParams->main.oscBlend);
    slPitchBendUp.setPointer(&pParams->main.pitchBendUpSemitones);
    slPitchBendDown.setPointer(&pParams->main.pitchBendDownSemitones);
    slAmpEgAttack.setPointer(&pParams->ampEG.attackTimeSeconds);
    slAmpEgDecay.setPointer(&pParams->ampEG.decayTimeSeconds);
    slAmpEgSustain.setPointer(&pParams->ampEG.sustainLevel);
    slAmpEgRelease.setPointer(&pParams->ampEG.releaseTimeSeconds);
}

void SARAHAudioProcessorEditor::comboBoxChanged(ComboBox*)
{
    processor.getSound()->parameterChanged();
}

void SARAHAudioProcessorEditor::sliderValueChanged(Slider*)
{
    processor.getSound()->parameterChanged();
}
