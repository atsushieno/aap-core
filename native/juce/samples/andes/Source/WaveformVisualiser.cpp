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

#include "../JuceLibraryCode/JuceHeader.h"
#include "WaveformVisualiser.h"

//==============================================================================
WaveformVisualiser::WaveformVisualiser(AndesAudioProcessor& processor) : processor(processor)
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
}

WaveformVisualiser::~WaveformVisualiser()
{
}

void WaveformVisualiser::paint(Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));

    g.setColour(Colours::darkgrey);

    g.fillRect(proportionOfWidth(0.25), 0, 1, getHeight());
    g.fillRect(proportionOfWidth(0.50), 0, 1, getHeight());
    g.fillRect(proportionOfWidth(0.75), 0, 1, getHeight());

    g.fillRect(0, proportionOfHeight(0.25), getWidth(), 1);
    g.fillRect(0, proportionOfHeight(0.50), getWidth(), 1);
    g.fillRect(0, proportionOfHeight(0.75), getWidth(), 1);

    g.setColour(Colours::darkgrey);
    g.drawRect(getLocalBounds(), 1);

    Path p;
    int w = getWidth();
    int h = getHeight();
    p.startNewSubPath(0.0f, h / 2.0f);
    for (int i = 1; i < w; ++i)
    {
        float z = ((float) i) / ((float) w) * 2.0f;

        p.lineTo(i, h / 2 - (processor.noise.fbm(
            z,
            (int) *processor.parameters.getRawParameterValue("octaves"),
            *processor.parameters.getRawParameterValue("persistence"),
            *processor.parameters.getRawParameterValue("offset"),
            *processor.parameters.getRawParameterValue("warping")
        ) * h / 2));
    }

    g.setColour(Colours::white);
    g.strokePath(p, PathStrokeType(1));
}

void WaveformVisualiser::resized()
{
    update();
}

void WaveformVisualiser::update()
{
    // TODO: create path here
    repaint();
}
