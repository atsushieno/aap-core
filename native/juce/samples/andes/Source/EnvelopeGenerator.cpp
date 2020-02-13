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

#include "EnvelopeGenerator.h"

EnvelopeGenerator::EnvelopeGenerator(AndesAudioProcessor& processor) : processor(processor)
{
    attackParam = processor.parameters.getRawParameterValue("env1att");
    decayParam = processor.parameters.getRawParameterValue("env1dec");
    sustainParam = processor.parameters.getRawParameterValue("env1sus");
    releaseParam = processor.parameters.getRawParameterValue("env1rel");
}

EnvelopeGenerator::~EnvelopeGenerator()
{
}

void EnvelopeGenerator::reset(double sampleRate)
{
    this->sampleRate = sampleRate;
    level = 0.0f;
    state = State::Attack;
}

void EnvelopeGenerator::release()
{
    releaseLevel = level;
    state = State::Release;
}

float EnvelopeGenerator::next()
{
    switch (state)
    {
        case State::Attack:
            level += 1.0 / (*attackParam * sampleRate);

            if (level >= 1.0f)
            {
                state = State::Decay;
            }
            break;

        case State::Decay:
            level -= (1.0 - *sustainParam) / (*decayParam * sampleRate);

            if (level <= *sustainParam)
            {
                state = State::Sustain;
            }
            break;

        case State::Sustain:
            level = *sustainParam;

            break;

        case State::Release:
            level -= releaseLevel / (*releaseParam * sampleRate);

            if (level <= 0)
            {
                level = 0;
                state = State::Done;
            }
            break;

        default:
            break;
    }

    return level;
}
