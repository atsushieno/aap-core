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

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginProcessor.h"
#include "AndesSlider.h"

//==============================================================================
/*
*/
class EnvelopeEditor    : public Component
{
public:
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

    EnvelopeEditor(AndesAudioProcessor& processor);
    ~EnvelopeEditor();

    void resized() override;

private:
    GroupComponent sliderGroup;

    AndesSlider attSlider;
    AndesSlider decSlider;
    AndesSlider susSlider;
    AndesSlider relSlider;

    Label attLabel;
    Label decLabel;
    Label susLabel;
    Label relLabel;

    ScopedPointer<SliderAttachment> attAttachment;
    ScopedPointer<SliderAttachment> decAttachment;
    ScopedPointer<SliderAttachment> susAttachment;
    ScopedPointer<SliderAttachment> relAttachment;

    AndesAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeEditor)
};
