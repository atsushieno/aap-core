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

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "AndesLookAndFeel.h"
#include "PluginProcessor.h"
#include "WaveformVisualiser.h"
#include "EnvelopeEditor.h"
#include "NoiseEditor.h"

class AndesAudioProcessorEditor : public AudioProcessorEditor,
                                  private Button::Listener,
                                  private MidiKeyboardStateListener,
                                  private Timer
{
public:
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

    AndesAudioProcessorEditor(AndesAudioProcessor&);
    ~AndesAudioProcessorEditor();

    //==============================================================================
    void paint(Graphics&) override;
    void resized() override;

    MidiKeyboardState keyboardState;
    MidiKeyboardComponent keyboardComponent;

private:
    AndesLookAndFeel andesLookAndFeel;
    AndesAudioProcessor& processor;

    TextEditor seedEditor;
    TextButton randomizeButton;

    WaveformVisualiser waveformVisualiser;
    EnvelopeEditor env1Editor;
    NoiseEditor noiseEditor;

    void buttonClicked(Button* button) override;
    void handleNoteOn(MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AndesAudioProcessorEditor)
};
