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

#include "AndesLookAndFeel.h"

AndesLookAndFeel::AndesLookAndFeel()
{
    setColourScheme(LookAndFeel_V4::getDarkColourScheme());
    setColour(Slider::rotarySliderFillColourId, Colours::white);
    setColour(Slider::textBoxOutlineColourId, Colours::transparentWhite);
}

void AndesLookAndFeel::drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float rotaryStartAngle, float rotaryEndAngle, Slider& slider)
{
    const auto bounds = Rectangle<int>(x, y, width, height).toFloat();

    auto radius = jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto lineW = jmin(4.0f, radius * 0.5f);
    auto arcRadius = radius - lineW * 0.5f;

    Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(),
                                bounds.getCentreY(),
                                arcRadius,
                                arcRadius,
                                0.0f,
                                rotaryStartAngle,
                                rotaryEndAngle,
                                true);

    g.setColour(slider.findColour(Slider::rotarySliderOutlineColourId));
    g.strokePath(backgroundArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));

    if (slider.isEnabled())
    {
        Path valueArc;
        valueArc.addCentredArc(bounds.getCentreX(),
                               bounds.getCentreY(),
                               arcRadius,
                               arcRadius,
                               0.0f,
                               rotaryStartAngle,
                               toAngle,
                               true);

        g.setColour(slider.findColour(Slider::rotarySliderFillColourId));
        g.strokePath(valueArc, PathStrokeType(lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const auto knobRadius = arcRadius - lineW * 1.5f;
    g.setColour(slider.findColour(Slider::rotarySliderOutlineColourId));
    g.fillEllipse(Rectangle<float>(knobRadius * 2, knobRadius * 2).withCentre(bounds.getCentre()));

    Path pointer;
    const float pointerLength = knobRadius * 0.5f;
    const float pointerThickness = 2.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -knobRadius, pointerThickness, pointerLength, pointerThickness / 2.0f);
    pointer.applyTransform(AffineTransform::rotation(toAngle).translated(bounds.getCentreX(), bounds.getCentreY()));
    g.setColour(slider.findColour(Slider::rotarySliderFillColourId));
    g.fillPath(pointer);
}
