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
#include "SynthParameters.h"

void SynthParameters::setDefaultValues()
{
    programName = "Default";
    main.masterLevel = 0.15f;
    main.oscBlend= 0.5f;
    main.pitchBendUpSemitones = 2;
    main.pitchBendDownSemitones = 2;

    osc1.pitchOffsetSemitones = 0;
    osc1.detuneOffsetCents = 0.0f;

    osc2.pitchOffsetSemitones = 0;
    osc2.detuneOffsetCents = 0.0f;

    ampEG.attackTimeSeconds = 0.1f;
    ampEG.decayTimeSeconds = 0.1f;
    ampEG.sustainLevel = 0.8f;
    ampEG.releaseTimeSeconds = 0.5f;

    filter1.cutoff = 1.0f;
    filter1.Q = 1.0f;
    filter1.envAmount = 0.0f;
    filter1EG.attackTimeSeconds = 0.0f;
    filter1EG.decayTimeSeconds = 0.0f;
    filter1EG.sustainLevel = 1.0f;
    filter1EG.releaseTimeSeconds = 0.0f;

    filter2.cutoff = 1.0f;
    filter2.Q = 1.0f;
    filter2.envAmount = 0.0f;
    filter2EG.attackTimeSeconds = 0.0f;
    filter2EG.decayTimeSeconds = 0.0f;
    filter2EG.sustainLevel = 1.0f;
    filter2EG.releaseTimeSeconds = 0.0f;

    pitch1EG.attackTimeSeconds = 0.0f;
    pitch1EG.decayTimeSeconds = 0.0f;
    pitch1EG.sustainLevel = 0.0f;
    pitch1EG.releaseTimeSeconds = 0.0f;

    pitch2EG.attackTimeSeconds = 0.0f;
    pitch2EG.decayTimeSeconds = 0.0f;
    pitch2EG.sustainLevel = 0.0f;
    pitch2EG.releaseTimeSeconds = 0.0f;

    pitchLFO.freqHz = 5.0f;
    pitchLFO.amount = 0.0f;

    filterLFO.freqHz = 5.0f;
    filterLFO.amount = 0.0f;
}

XmlElement* SynthParameters::getXml()
{
    XmlElement* xml = new XmlElement("program");

    xml->setAttribute("name", programName);

    xml->setAttribute("masterLevel", main.masterLevel);
    xml->setAttribute("oscBlend", main.oscBlend);
    xml->setAttribute("pitchBendUpSemitones", main.pitchBendUpSemitones);
    xml->setAttribute("pitchBendDownSemitones", main.pitchBendDownSemitones);

    xml->setAttribute("osc1Waveform", osc1.waveform.name());
    xml->setAttribute("osc1PitchOffsetSemitones", osc2.pitchOffsetSemitones);
    xml->setAttribute("osc1DetuneOffsetCents", osc1.detuneOffsetCents);

    xml->setAttribute("osc2Waveform", osc2.waveform.name());
    xml->setAttribute("osc2PitchOffsetSemitones", osc2.pitchOffsetSemitones);
    xml->setAttribute("osc2DetuneOffsetCents", osc2.detuneOffsetCents);

    xml->setAttribute("ampEgAttackTimeSeconds", ampEG.attackTimeSeconds);
    xml->setAttribute("ampEgDecayTimeSeconds", ampEG.decayTimeSeconds);
    xml->setAttribute("ampEgSustainLevel", ampEG.sustainLevel);
    xml->setAttribute("ampEgReleaseTimeSeconds", ampEG.releaseTimeSeconds);

    xml->setAttribute("flt1Cutoff", filter1.cutoff);
    xml->setAttribute("flt1Q", filter1.Q);
    xml->setAttribute("flt1EnvAmount", filter1.envAmount);
    xml->setAttribute("flt1EgAttackTimeSeconds", filter1EG.attackTimeSeconds);
    xml->setAttribute("flt1EgDecayTimeSeconds", filter1EG.decayTimeSeconds);
    xml->setAttribute("flt1EgSustainLevel", filter1EG.sustainLevel);
    xml->setAttribute("flt1EgReleaseTimeSeconds", filter1EG.releaseTimeSeconds);

    xml->setAttribute("flt2Cutoff", filter2.cutoff);
    xml->setAttribute("flt2Q", filter2.Q);
    xml->setAttribute("flt2EnvAmount", filter2.envAmount);
    xml->setAttribute("flt2EgAttackTimeSeconds", filter2EG.attackTimeSeconds);
    xml->setAttribute("flt2EgDecayTimeSeconds", filter2EG.decayTimeSeconds);
    xml->setAttribute("flt2EgSustainLevel", filter2EG.sustainLevel);
    xml->setAttribute("flt2EgReleaseTimeSeconds", filter2EG.releaseTimeSeconds);

    xml->setAttribute("pitchLfoWaveform", pitchLFO.waveform.name());
    xml->setAttribute("pitchLfoFreqHz", pitchLFO.freqHz);
    xml->setAttribute("pitchLfoAmount", pitchLFO.amount);

    xml->setAttribute("filterLfoWaveform", filterLFO.waveform.name());
    xml->setAttribute("filterLfoFreqHz", filterLFO.freqHz);
    xml->setAttribute("filterLfoAmount", filterLFO.amount);

    return xml;
}

void SynthParameters::putXml(XmlElement* xml)
{
    programName = xml->getStringAttribute("name");

    main.masterLevel = (float)xml->getDoubleAttribute("masterLevel");
    main.oscBlend = (float)xml->getDoubleAttribute("oscBlend");
    main.pitchBendUpSemitones = xml->getIntAttribute("pitchBendUpSemitones");
    main.pitchBendDownSemitones = xml->getIntAttribute("pitchBendDownSemitones");

    osc1.waveform.setFromName(xml->getStringAttribute("osc1Waveform"));
    osc2.pitchOffsetSemitones = xml->getIntAttribute("osc1PitchOffsetSemitones");
    osc1.detuneOffsetCents = (float)xml->getDoubleAttribute("osc1DetuneOffsetCents");

    osc2.waveform.setFromName(xml->getStringAttribute("osc2Waveform"));
    osc2.pitchOffsetSemitones = xml->getIntAttribute("osc2PitchOffsetSemitones");
    osc2.detuneOffsetCents = (float)xml->getDoubleAttribute("osc2DetuneOffsetCents");

    ampEG.attackTimeSeconds = (float)xml->getDoubleAttribute("ampEgAttackTimeSeconds");
    ampEG.decayTimeSeconds = (float)xml->getDoubleAttribute("ampEgDecayTimeSeconds");
    ampEG.sustainLevel = (float)xml->getDoubleAttribute("ampEgSustainLevel");
    ampEG.releaseTimeSeconds = (float)xml->getDoubleAttribute("ampEgReleaseTimeSeconds");

    filter1.cutoff = (float)xml->getDoubleAttribute("flt1Cutoff");
    filter1.Q = (float)xml->getDoubleAttribute("flt1Q");
    filter1.envAmount = (float)xml->getDoubleAttribute("flt1EnvAmount");
    filter1EG.attackTimeSeconds = (float)xml->getDoubleAttribute("flt1EgAttackTimeSeconds");
    filter1EG.decayTimeSeconds = (float)xml->getDoubleAttribute("flt1EgDecayTimeSeconds");
    filter1EG.sustainLevel = (float)xml->getDoubleAttribute("flt1EgSustainLevel");
    filter1EG.releaseTimeSeconds = (float)xml->getDoubleAttribute("flt1EgReleaseTimeSeconds");

    filter2.cutoff = (float)xml->getDoubleAttribute("flt2Cutoff");
    filter2.Q = (float)xml->getDoubleAttribute("flt2Q");
    filter2.envAmount = (float)xml->getDoubleAttribute("flt2EnvAmount");
    filter2EG.attackTimeSeconds = (float)xml->getDoubleAttribute("flt2EgAttackTimeSeconds");
    filter2EG.decayTimeSeconds = (float)xml->getDoubleAttribute("flt2EgDecayTimeSeconds");
    filter2EG.sustainLevel = (float)xml->getDoubleAttribute("flt2EgSustainLevel");
    filter2EG.releaseTimeSeconds = (float)xml->getDoubleAttribute("flt2EgReleaseTimeSeconds");

    pitchLFO.waveform.setFromName(xml->getStringAttribute("pitchLfoWaveform"));
    pitchLFO.freqHz = (float)xml->getIntAttribute("pitchLfoFreqHz");
    pitchLFO.amount = (float)xml->getDoubleAttribute("pitchLfoAmount");

    filterLFO.waveform.setFromName(xml->getStringAttribute("filterLfoWaveform"));
    filterLFO.freqHz = (float)xml->getIntAttribute("filterLfoFreqHz");
    filterLFO.amount = (float)xml->getDoubleAttribute("filterLfoAmount");
}
