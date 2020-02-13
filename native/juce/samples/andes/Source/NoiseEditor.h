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
class NoiseEditor    : public Component
{
public:
    typedef AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

    NoiseEditor(AndesAudioProcessor& processor);
    ~NoiseEditor();

    void resized() override;

private:
    GroupComponent sliderGroup;

    AndesSlider warpSlider;
    AndesSlider offsetSlider;
    AndesSlider octavesSlider;
    AndesSlider persistenceSlider;

    Label warpLabel;
    Label offsetLabel;
    Label octavesLabel;
    Label persistenceLabel;

    ScopedPointer<SliderAttachment> warpAttachment;
    ScopedPointer<SliderAttachment> offsetAttachment;
    ScopedPointer<SliderAttachment> octavesAttachment;
    ScopedPointer<SliderAttachment> persistenceAttachment;

    AndesAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseEditor)
};
