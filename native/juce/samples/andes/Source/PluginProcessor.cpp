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
#include "AndesSound.h"
#include "AndesVoice.h"

//==============================================================================
AndesAudioProcessor::AndesAudioProcessor()
     : AudioProcessor(BusesProperties().withOutput("Output", AudioChannelSet::stereo(), true)), parameters(*this, nullptr)
{
    parameters.createAndAddParameter("octaves", "octaves", String(),
                                     NormalisableRange<float> (1.0f, 16.0f, 1.0f),
                                     1,
                                     nullptr,
                                     nullptr);

    parameters.createAndAddParameter("persistence", "persistence", String(),
                                     NormalisableRange<float> (0.0f, 1.0f),
                                     0.5f,
                                     nullptr,
                                     nullptr);

    parameters.createAndAddParameter("env1att", "env1att", String(),
                                     NormalisableRange<float> (0.001f, 9.0f, 0, 0.55),
                                     0.005f,
                                     nullptr,
                                     nullptr);

    parameters.createAndAddParameter("env1dec", "env1dec", String(),
                                     NormalisableRange<float> (0.001f, 9.0f, 0, 0.55),
                                     0.2f,
                                     nullptr,
                                     nullptr);

    parameters.createAndAddParameter("env1sus", "env1sus", String(),
                                     NormalisableRange<float> (0.0f, 1.0f),
                                     0.5f,
                                     nullptr,
                                     nullptr);

    parameters.createAndAddParameter("env1rel", "env1rel", String(),
                                     NormalisableRange<float> (0.001f, 9.0f, 0, 0.55),
                                     0.050f,
                                     nullptr,
                                     nullptr);

    parameters.createAndAddParameter("offset", "offset", String(),
                                     NormalisableRange<float> (0.0f, 4.0f),
                                     0.0f,
                                     nullptr,
                                     nullptr);

    parameters.createAndAddParameter("warping", "warping", String(),
                                     NormalisableRange<float> (0.0f, 1.0f),
                                     0.0f,
                                     nullptr,
                                     nullptr);

    parameters.state = ValueTree(Identifier("AndesProgram"));
    parameters.state.setProperty("version", 0, nullptr);
    internalParameters = parameters.state.getOrCreateChildWithName("Internal", nullptr);

    synth.clearVoices();
    synth.clearSounds();

    for (int i = 8; --i >= 0; ) {
        synth.addVoice(new AndesVoice(*this));
    }

    synth.addSound(new AndesSound());
}

AndesAudioProcessor::~AndesAudioProcessor()
{
}

//==============================================================================
const String AndesAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AndesAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AndesAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

double AndesAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AndesAudioProcessor::getNumPrograms()
{
    return 100;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AndesAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AndesAudioProcessor::setCurrentProgram(int index)
{
}

const String AndesAudioProcessor::getProgramName(int index)
{
    return "Default";
}

void AndesAudioProcessor::changeProgramName(int index, const String& newName)
{
}

//==============================================================================
void AndesAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void AndesAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AndesAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused(layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AndesAudioProcessor::processBlock(AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (AudioProcessorEditor* editor = getActiveEditor())
    {
        if (AndesAudioProcessorEditor* andesEditor = dynamic_cast<AndesAudioProcessorEditor*> (editor))
        {
            andesEditor->keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);
        }
    }

    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

//==============================================================================
bool AndesAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* AndesAudioProcessor::createEditor()
{
    return new AndesAudioProcessorEditor(*this);
}

//==============================================================================
void AndesAudioProcessor::getStateInformation(MemoryBlock& destData)
{
    internalParameters.setProperty("seed", (int64) noise.getSeed(), nullptr);
    std::unique_ptr<XmlElement> xml(parameters.state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AndesAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            parameters.state = ValueTree::fromXml(*xmlState);
            internalParameters = parameters.state.getOrCreateChildWithName("Internal", nullptr);
            noise.setSeed((uint32_t) (int64) internalParameters.getProperty("seed"));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AndesAudioProcessor();
}
