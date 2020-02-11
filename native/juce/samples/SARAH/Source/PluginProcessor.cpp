/* Copyright (c) 2017-2018 Shane D. Dunne

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SynthVoice.h"

SARAHAudioProcessor::SARAHAudioProcessor()
    : currentProgram(0)
{
    SynthOscillatorBase::Initialize();
    initializePrograms();

    //formatManager.registerBasicFormats();

    for (int i = 0; i < kNumberOfVoices; ++i)
        //synth.addVoice(new SamplerVoice());
        synth.addVoice(new SynthVoice());

    //loadNewSample(BinaryData::singing_ogg, BinaryData::singing_oggSize);
    pSound = new SynthSound(synth);
    pSound->pParams = &programBank[currentProgram];
    synth.addSound(pSound);
}

SARAHAudioProcessor::~SARAHAudioProcessor()
{
    SynthOscillatorBase::Cleanup();
}

void SARAHAudioProcessor::initializePrograms()
{
    for (int i = 0; i < kNumberOfPrograms; i++)
        programBank[i].setDefaultValues();
}

#if 0
void SARAHAudioProcessor::loadNewSample(const void* data, int dataSize)
{
    MemoryInputStream* soundBuffer = new MemoryInputStream(data, static_cast<std::size_t> (dataSize), false);
    ScopedPointer<AudioFormatReader> formatReader(formatManager.findFormatForFileExtension("ogg")->createReaderFor(soundBuffer, true));

    BigInteger midiNotes;
    midiNotes.setRange(0, 126, true);
    SynthesiserSound::Ptr newSound = new SamplerSound("Voice", *formatReader, midiNotes, 0x40, 0.0, 0.0, 10.0);

    synth.removeSound(0);
    sound = newSound;
    synth.addSound(sound);
}
#endif


const String SARAHAudioProcessor::getName() const
{
    return String(JucePlugin_Name);
}

bool SARAHAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SARAHAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

double SARAHAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SARAHAudioProcessor::getNumPrograms()
{
    return kNumberOfPrograms;
}

int SARAHAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void SARAHAudioProcessor::setCurrentProgram (int index)
{
    currentProgram = index;
    pSound->pParams = &programBank[currentProgram];
    sendChangeMessage();
}

const String SARAHAudioProcessor::getProgramName (int index)
{
    return programBank[index].programName;
}

void SARAHAudioProcessor::changeProgramName (int index, const String& newName)
{
    programBank[index].programName = newName;
    sendChangeMessage();
}

void SARAHAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    ignoreUnused(samplesPerBlock);

    synth.setCurrentPlaybackSampleRate(sampleRate);
}

void SARAHAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void SARAHAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    synth.renderNextBlock(buffer, midiMessages, 0, buffer.getNumSamples());
}

bool SARAHAudioProcessor::hasEditor() const
{
    return true;
}

AudioProcessorEditor* SARAHAudioProcessor::createEditor()
{
    return new SARAHAudioProcessorEditor(*this);
}

void SARAHAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    XmlElement xml = XmlElement("SARAH");
    xml.setAttribute(String("currentProgram"), currentProgram);
    XmlElement* xprogs = new XmlElement("programs");
    for (int i = 0; i < kNumberOfPrograms; i++)
        xprogs->addChildElement(programBank[i].getXml());
    xml.addChildElement(xprogs);
    copyXmlToBinary(xml, destData);
}

void SARAHAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xml = getXmlFromBinary(data, sizeInBytes);
    XmlElement* xprogs = xml->getFirstChildElement();
    if (xprogs->hasTagName(String("programs")))
    {
        int i = 0;
        forEachXmlChildElement(*xprogs, xpr)
        {
            programBank[i].setDefaultValues();
            programBank[i].putXml(xpr);
            i++;
        }
    }
    setCurrentProgram(xml->getIntAttribute(String("currentProgram"), 0));
}

void SARAHAudioProcessor::getCurrentProgramStateInformation(MemoryBlock& destData)
{
    ScopedPointer<XmlElement> xml = programBank[currentProgram].getXml();
    copyXmlToBinary(*xml, destData);
}

void SARAHAudioProcessor::setCurrentProgramStateInformation(const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xml = getXmlFromBinary(data, sizeInBytes);
    programBank[currentProgram].putXml(xml);
    sendChangeMessage();
}

// This creates new instances of the plugin.
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SARAHAudioProcessor();
}
