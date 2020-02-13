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

#include "AndesVoice.h"
#include "AndesSound.h"

AndesVoice::AndesVoice(AndesAudioProcessor& processor)
    : currentPhase(0), phaseDelta(0), level(0),
      processor(processor),
      envGen(processor)
{
}

bool AndesVoice::canPlaySound(SynthesiserSound* sound)
{
    return dynamic_cast<AndesSound*> (sound) != nullptr;
}

void AndesVoice::startNote(int midiNoteNumber, float velocity, SynthesiserSound *sound, int currentPitchWheelPosition)
{
    currentPhase = 0.0;
    level = velocity;

    frequency = MidiMessage::getMidiNoteInHertz(midiNoteNumber);
    maxFrequency = MidiMessage::getMidiNoteInHertz(midiNoteNumber + 4);
    minFrequency = MidiMessage::getMidiNoteInHertz(midiNoteNumber - 4);
    normalizedFrequency = frequency / getSampleRate();

    phaseDelta = normalizedFrequency * 2.0;
    pitchWheelMoved(currentPitchWheelPosition);

    envGen.reset(getSampleRate());
}

void AndesVoice::stopNote(float velocity, bool allowTailOff)
{
    if (allowTailOff)
    {
        envGen.release();
    }
    else
    {
        // we're being told to stop playing immediately, so reset everything..

        clearCurrentNote();
        phaseDelta = 0.0;
    }
}

void AndesVoice::pitchWheelMoved(int newPitchWheelValue)
{
    double frequencyOffset = (newPitchWheelValue / 8192.0f) - 1.0f;
    double newFrequency;

    if (frequencyOffset < 0)
    {
        newFrequency = frequency + ((frequency - minFrequency) * frequencyOffset);
    }
    else
    {
        newFrequency = frequency + ((maxFrequency - frequency) * frequencyOffset);
    }
    phaseDelta = newFrequency / getSampleRate() * 2.0;
}

void AndesVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
}

void AndesVoice::renderNextBlock(AudioSampleBuffer& outputBuffer, int startSample, int numSamples)
{
    if (phaseDelta != 0.0)
    {
        while (--numSamples >= 0)
        {
            float phase = fmod(currentPhase, 2);
            const float envLevel = envGen.next();
            const float currentSample = processor.noise.fbm(phase,
                                                            (int) *processor.parameters.getRawParameterValue("octaves"),
                                                            *processor.parameters.getRawParameterValue("persistence"),
                                                            *processor.parameters.getRawParameterValue("offset"),
                                                            *processor.parameters.getRawParameterValue("warping")) * level * envLevel;

            for (int i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample(i, startSample, currentSample);

            currentPhase += phaseDelta;
            ++startSample;

            if (envGen.state == EnvelopeGenerator::Done)
            {
                // tells the synth that this voice has stopped
                clearCurrentNote();

                phaseDelta = 0.0;
                break;
            }
        }
    }
}
