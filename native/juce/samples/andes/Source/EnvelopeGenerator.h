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

#include "PluginProcessor.h"

class EnvelopeGenerator
{
public:
    EnvelopeGenerator(AndesAudioProcessor& processor);
    ~EnvelopeGenerator();

    void reset(double sampleRate);
    void release();
    float next();

    enum State {
        Attack,
        Decay,
        Sustain,
        Release,
        Done
    };

    State state;

private:
    AndesAudioProcessor& processor;

    std::atomic<float>* attackParam;
    std::atomic<float>* decayParam;
    std::atomic<float>* sustainParam;
    std::atomic<float>* releaseParam;

    double sampleRate;
    float level;
    float releaseLevel;
};
