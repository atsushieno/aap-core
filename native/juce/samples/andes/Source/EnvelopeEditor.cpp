/*
    Andes - synthesiser plugin based on Perlin EnvelopeGenerator
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

#include "../JuceLibraryCode/JuceHeader.h"
#include "EnvelopeEditor.h"

//==============================================================================
EnvelopeEditor::EnvelopeEditor(AndesAudioProcessor& processor) : processor(processor)
{
    sliderGroup.setText("Envelope");
    addAndMakeVisible(sliderGroup);

    attSlider.setSliderStyle(Slider::RotaryVerticalDrag);
    attSlider.setTextBoxStyle(Slider::TextBoxBelow, true, 50, 15);
    attSlider.setGetTextFromValueFunc([](double value) { return String(round(value * 1000)) + " ms"; });
    sliderGroup.addAndMakeVisible(&attSlider);
    attAttachment = new SliderAttachment(processor.parameters, "env1att", attSlider);


    decSlider.setSliderStyle(Slider::RotaryVerticalDrag);
    decSlider.setTextBoxStyle(Slider::TextBoxBelow, true, 50, 15);
    decSlider.setGetTextFromValueFunc([](double value) { return String(round(value * 1000)) + " ms"; });
    sliderGroup.addAndMakeVisible(&decSlider);
    decAttachment = new SliderAttachment(processor.parameters, "env1dec", decSlider);

    susSlider.setSliderStyle(Slider::RotaryVerticalDrag);
    susSlider.setTextBoxStyle(Slider::TextBoxBelow, true, 50, 15);
    susSlider.setGetTextFromValueFunc([](double value) { return String(round(value * 100)) + "%"; });
    sliderGroup.addAndMakeVisible(&susSlider);
    susAttachment = new SliderAttachment(processor.parameters, "env1sus", susSlider);

    relSlider.setSliderStyle(Slider::RotaryVerticalDrag);
    relSlider.setTextBoxStyle(Slider::TextBoxBelow, true, 50, 15);
    relSlider.setGetTextFromValueFunc([](double value) { return String(round(value * 1000)) + " ms"; });
    sliderGroup.addAndMakeVisible(&relSlider);
    relAttachment = new SliderAttachment(processor.parameters, "env1rel", relSlider);

    attLabel.setText("Attack", NotificationType::dontSendNotification);
    attLabel.setJustificationType(Justification::centred);
    sliderGroup.addAndMakeVisible(&attLabel);

    decLabel.setText("Decay", NotificationType::dontSendNotification);
    decLabel.setJustificationType(Justification::centred);
    sliderGroup.addAndMakeVisible(&decLabel);

    susLabel.setText("Sustain", NotificationType::dontSendNotification);
    susLabel.setJustificationType(Justification::centred);
    sliderGroup.addAndMakeVisible(&susLabel);

    relLabel.setText("Release", NotificationType::dontSendNotification);
    relLabel.setJustificationType(Justification::centred);
    sliderGroup.addAndMakeVisible(&relLabel);
}

EnvelopeEditor::~EnvelopeEditor()
{
}

void EnvelopeEditor::resized()
{
    sliderGroup.setBounds(0, 0, getWidth(), getHeight());

    int sliderY = 15;
    int sliderHeight = sliderGroup.getHeight() - 40;
    int sliderWidth = sliderGroup.getWidth() / 4;

    attSlider.setBounds(sliderWidth * 0, sliderY, sliderWidth, sliderHeight);
    decSlider.setBounds(sliderWidth * 1, sliderY, sliderWidth, sliderHeight);
    susSlider.setBounds(sliderWidth * 2, sliderY, sliderWidth, sliderHeight);
    relSlider.setBounds(sliderWidth * 3, sliderY, sliderWidth, sliderHeight);

    attLabel.setBounds(sliderWidth * 0, sliderY + sliderHeight + 5, sliderWidth, 10);
    decLabel.setBounds(sliderWidth * 1, sliderY + sliderHeight + 5, sliderWidth, 10);
    susLabel.setBounds(sliderWidth * 2, sliderY + sliderHeight + 5, sliderWidth, 10);
    relLabel.setBounds(sliderWidth * 3, sliderY + sliderHeight + 5, sliderWidth, 10);
}
