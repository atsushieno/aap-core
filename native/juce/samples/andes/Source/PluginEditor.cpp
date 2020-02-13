/*
    Andes - synthesiser plugin based on Perlin noise
    Copyright (C) 2017  Artem Popov <art@artfwo.net>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <sstream>
#include <iomanip>

//==============================================================================
AndesAudioProcessorEditor::AndesAudioProcessorEditor(AndesAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p),
      waveformVisualiser(processor),
      env1Editor(processor),
      noiseEditor(processor),
      keyboardComponent(keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    LookAndFeel::setDefaultLookAndFeel(&andesLookAndFeel);
    setSize(500, 314);

    seedEditor.setReadOnly(true); // TODO: actually should be editable
    seedEditor.setText(String::toHexString((int) processor.noise.getSeed()));

    randomizeButton.addListener(this);
    randomizeButton.setButtonText("Randomize");

    addAndMakeVisible(&seedEditor);
    addAndMakeVisible(&randomizeButton);
    addAndMakeVisible(&waveformVisualiser);
    addAndMakeVisible(&env1Editor);
    addAndMakeVisible(&noiseEditor);

    keyboardComponent.setWantsKeyboardFocus(true);
    addAndMakeVisible(&keyboardComponent);

    sendLookAndFeelChange(); // to re-create slider labels with proper colours

    startTimerHz(30);
    srand(time(NULL));
}

AndesAudioProcessorEditor::~AndesAudioProcessorEditor()
{
}

//==============================================================================
void AndesAudioProcessorEditor::paint(Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
}

void AndesAudioProcessorEditor::resized()
{
    Rectangle<int> area(getLocalBounds());

    const int keyboardHeight = 64;
    keyboardComponent.setBounds(area.removeFromBottom(keyboardHeight));

    area.reduce(10, 10);

    {
        Rectangle<int> displayArea(area.removeFromRight(jmax(170, area.getWidth() / 3)).reduced(5));
        Rectangle<int> buttonsArea(displayArea.removeFromBottom(26));

        seedEditor.setBounds(buttonsArea.removeFromLeft(buttonsArea.getWidth() / 2).withTrimmedRight(5));
        randomizeButton.setBounds(buttonsArea.withTrimmedLeft(5));
        waveformVisualiser.setBounds(displayArea.withTrimmedBottom(10));
    }

    noiseEditor.setBounds(area.removeFromTop(area.getHeight() / 2).reduced(5));
    env1Editor.setBounds(area.reduced(5));
}

void AndesAudioProcessorEditor::buttonClicked(Button* button)
{
    if (button == &randomizeButton)
    {
        uint32_t seed = rand();
        processor.noise.setSeed(seed);

        seedEditor.setText(String::toHexString((int) seed));
        waveformVisualiser.update();
    }
}

void AndesAudioProcessorEditor::handleNoteOn(MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity)
{

}

void AndesAudioProcessorEditor::handleNoteOff(MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity)
{

}

void AndesAudioProcessorEditor::timerCallback()
{
    waveformVisualiser.update();
    seedEditor.setText(String::toHexString((int) processor.noise.getSeed()));
}
