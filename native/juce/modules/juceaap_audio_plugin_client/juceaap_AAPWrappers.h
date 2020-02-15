
#include "../JuceLibraryCode/JuceHeader.h"
#include "../../plugin-api/include/aap/android-audio-plugin.h"

extern juce::AudioProcessor* createPluginFilter(); // it is defined in each Audio plugin project (by Projucer).

extern "C" int juce_aap_wrapper_last_error_code{0};

#define JUCEAAP_SUCCESS 0
#define JUCEAAP_ERROR_INVALID_BUFFER -1
#define JUCEAAP_ERROR_PROCESS_BUFFER_ALTERED -2


// JUCE-AAP port mappings:
//
//  JUCE parameters (flattened) 0..p-1 -> AAP ports 0..p-1
// 	JUCE AudioBuffer 0..nIn-1 -> AAP input ports p..p+nIn-1
//  JUCE AudioBuffer nIn..nIn+nOut-1 -> AAP output ports p+nIn..p+nIn+nOut-1
//  IF exists JUCE MIDI input buffer -> AAP MIDI input port p+nIn+nOut
//  IF exists JUCE MIDI output buffer -> AAP MIDI output port last

class JuceAAPWrapper {
	AndroidAudioPlugin *aap;
	const char* plugin_unique_id;
	int sample_rate;
	const AndroidAudioPluginExtension * const *extensions;
	AndroidAudioPluginBuffer *buffer;
    AndroidAudioPluginState state;
	juce::AudioProcessor *juce_processor;
	juce::AudioBuffer<float> juce_buffer;
	juce::MidiBuffer juce_midi_messages;

public:
	JuceAAPWrapper(AndroidAudioPlugin *plugin, const char* pluginUniqueId, int sampleRate, const AndroidAudioPluginExtension * const *extensions)
		: aap(plugin), plugin_unique_id(pluginUniqueId == nullptr ? nullptr : strdup(pluginUniqueId)), sample_rate(sampleRate), extensions(extensions)
	{
        juce_processor = createPluginFilter();
	}

	virtual ~JuceAAPWrapper()
	{
		if (plugin_unique_id != nullptr)
			free((void*) plugin_unique_id);
	}

	void allocateBuffer(AndroidAudioPluginBuffer* buffer)
    {
        if (!buffer) {
            errno = juce_aap_wrapper_last_error_code = JUCEAAP_ERROR_INVALID_BUFFER;
            return;
        }
        // allocates juce_buffer. No need to interpret content.
        this->buffer = buffer;
        juce_buffer.setSize(juce_processor->getMainBusNumInputChannels() + juce_processor->getMainBusNumOutputChannels(), buffer->num_frames);
        juce_midi_messages.clear();
    }

	void prepare(AndroidAudioPluginBuffer* buffer)
	{
	    allocateBuffer(buffer);
	    if (juce_aap_wrapper_last_error_code != JUCEAAP_SUCCESS)
	        return;
	}

	void activate()
	{
        juce_processor->prepareToPlay(sample_rate, buffer->num_frames);
	}

	void deactivate()
	{
	    juce_processor->releaseResources();
	}

	int32_t current_bpm = 120; // FIXME: provide way to adjust it
	int32_t default_time_division = 192;

	// FIXME: process() should not really pass audioBuffer.
	//  It must be always deactivated, re-prepared, and re-activated.
	void process(AndroidAudioPluginBuffer* audioBuffer, long timeoutInNanoseconds)
	{
        if (audioBuffer != buffer) {
            JUCEAAP_ERROR_PROCESS_BUFFER_ALTERED;
            return;
        }
        int nPara = juce_processor->getParameters().size();
        // parameters should be filled whenever JUCE parameter is assigned, so no need to process here.
        // FIXME: implement above.

        int nIn = juce_processor->getMainBusNumInputChannels();
		int nBuf = juce_processor->getMainBusNumInputChannels() + juce_processor->getMainBusNumOutputChannels();
        for (int i = 0; i < nIn; i++)
        	memcpy((void *) juce_buffer.getReadPointer(i), audioBuffer->buffers[i + nPara], sizeof(float) * audioBuffer->num_frames);
        int outputTimeDivision = default_time_division;

        if (juce_processor->acceptsMidi()) {
        	juce_midi_messages.clear();
        	void* src = audioBuffer->buffers[nPara + nBuf];
        	uint8_t* csrc = (uint8_t*) src;
        	int* isrc = (int*) src;
        	int32_t timeDivision = outputTimeDivision = isrc[0];
			uint8_t ticksPerFrame = timeDivision & 0xFF; // if it's in SMTPE.
        	int32_t srcEnd = isrc[1] + 8;
        	int32_t srcN = 8;
			uint8_t running_status = 0;
        	while (srcN < srcEnd) {
				long timecode = 0;
				int digits = 0;
				while (csrc[srcN] >= 0x80 && srcN < srcEnd) // variable length
					timecode += ((csrc[srcN++] - 0x80) << (7 * digits++));
				if (srcN == srcEnd)
					break; // invalid data
				timecode += (csrc[srcN++] << (7 * digits));

				int32_t srcEventStart = srcN;
				uint8_t statusByte = csrc[srcN] >= 0x80 ? csrc[srcN] : running_status;
				running_status = statusByte;
				uint8_t eventType = statusByte & 0xF0;
				uint32_t midiEventSize = 3;
				int sysexPos = srcN;
				switch (eventType) {
					case 0xF0:
						midiEventSize = 2; // F0 + F7
						while (csrc[sysexPos++] != 0xF7 && sysexPos < srcEnd)
							midiEventSize++;
						break;
					case 0xC0: case 0xD0: case 0xF1: case 0xF3: case 0xF9: midiEventSize = 2; break;
					case 0xF6: case 0xF7: midiEventSize = 1; break;
					default:
						if (eventType > 0xF8)
							midiEventSize = 1;
						break;
				}

				double timestamp;
				// FIXME: we will have to revisit here later...
				if(timeDivision > 0x7FFF) {
					timestamp = timecode * 1.0f / sample_rate / ticksPerFrame;
				} else {
					timestamp = 60.0f / current_bpm * timecode / timeDivision;
				}
				MidiMessage m;
				bool skip = false;
				switch (midiEventSize) {
				case 1:
					skip = true; // there is no way to create MidiMessage for F6 (tune request) and F7 (end of sysex)
					break;
				case 2:
				    m = MidiMessage{csrc[srcEventStart], csrc[srcEventStart + 1], timestamp};
				    break;
				case 3:
				    m = MidiMessage{csrc[srcEventStart], csrc[srcEventStart + 1], csrc[srcEventStart + 2], timestamp};
                    break;
				default: // sysex etc.
					m = MidiMessage::createSysExMessage(csrc + srcEventStart, midiEventSize);
                    break;
				}
				if (!skip) {
					m.setChannel((statusByte & 0x0F) + 1); // they accept 1..16, not 0..15...
					juce_midi_messages.addEvent(m, 0);
				}
				srcN += midiEventSize;
			}
        }

        // process data by the JUCE plugin
        juce_processor->processBlock(juce_buffer, juce_midi_messages);

		for (int i = juce_processor->getMainBusNumInputChannels(); i < nBuf; i++)
			memcpy(audioBuffer->buffers[i + nPara], (void *) juce_buffer.getReadPointer(i), sizeof(float) * audioBuffer->num_frames);

		if(juce_processor->producesMidi()) {
			int32_t bufIndex = nPara + nBuf + (juce_processor->acceptsMidi() ? 1 : 0);
			void *dst = buffer->buffers[bufIndex];
			uint8_t *cdst = (uint8_t*) dst;
			int* idst = (int*) dst;
			MidiBuffer::Iterator iterator{juce_midi_messages};
			const uint8_t* data;
			int32_t eventSize, eventPos, dstBufSize = 0;
			while (iterator.getNextEvent(data, eventSize, eventPos)) {
				memcpy(cdst + 8 + dstBufSize, data + eventPos, eventSize);
				dstBufSize += eventSize;
			}
			idst[0] = outputTimeDivision;
			idst[1] = dstBufSize;
		}
	}

	// FIXME: we should probably make changes to getState() signature to not return raw pointer,
	//  as it expects that memory allocation runs at plugins. It should be passed by argument and
	//  allocated externally. JUCE does it better.
	AndroidAudioPluginState* getState()
	{
	    MemoryBlock mb;
	    mb.reset();
	    juce_processor->getStateInformation(mb);
        memcpy((void*) state.raw_data, mb.begin(), mb.getSize());
        return &state;
	}

	void setState(AndroidAudioPluginState* input)
	{
	    juce_processor->setStateInformation(input->raw_data, input->data_size);
	}
};

JuceAAPWrapper* getWrapper(AndroidAudioPlugin* plugin)
{
	return (JuceAAPWrapper*) plugin->plugin_specific;
}

void juceaap_prepare(
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* audioBuffer)
{
	getWrapper(plugin)->prepare(audioBuffer);
}

void juceaap_activate(AndroidAudioPlugin *plugin)
{
	getWrapper(plugin)->activate();
}

void juceaap_deactivate(AndroidAudioPlugin *plugin)
{
	getWrapper(plugin)->deactivate();
}

void juceaap_process(
	AndroidAudioPlugin *plugin,
	AndroidAudioPluginBuffer* audioBuffer,
	long timeoutInNanoseconds)
{
	getWrapper(plugin)->process(audioBuffer, timeoutInNanoseconds);
}

const AndroidAudioPluginState* juceaap_get_state(AndroidAudioPlugin *plugin)
{
	return getWrapper(plugin)->getState();
}

void juceaap_set_state(AndroidAudioPlugin *plugin, AndroidAudioPluginState *input)
{
	getWrapper(plugin)->setState(input);
}

AndroidAudioPlugin* juceaap_instantiate(
	AndroidAudioPluginFactory *pluginFactory,
	const char* pluginUniqueId,
	int sampleRate,
	const AndroidAudioPluginExtension * const *extensions)
{
	auto *ret = new AndroidAudioPlugin();
	auto *ctx = new JuceAAPWrapper(ret, pluginUniqueId, sampleRate, extensions);

	ret->plugin_specific = ctx;

	ret->prepare = juceaap_prepare;
	ret->activate = juceaap_activate;
	ret->process = juceaap_process;
	ret->deactivate = juceaap_deactivate;
	ret->get_state = juceaap_get_state;
	ret->set_state = juceaap_set_state;

	return ret;
}

void juceaap_release(
	AndroidAudioPluginFactory *pluginFactory,
	AndroidAudioPlugin *instance)
{
	auto ctx = getWrapper(instance);
	if (ctx != nullptr) {
		delete ctx;
		instance->plugin_specific = nullptr;
	}
	delete instance;
}

struct AndroidAudioPluginFactory juceaap_factory{
	juceaap_instantiate,
	juceaap_release
};

extern "C" AndroidAudioPluginFactory* GetJuceAAPFactory()
{
  return &juceaap_factory;
}

// below are used by external metadata generator tool

#define JUCEAAP_EXPORT_AAP_METADATA_SUCCESS 0
#define JUCEAAP_EXPORT_AAP_METADATA_INVALID_DIRECTORY 1
#define JUCEAAP_EXPORT_AAP_METADATA_INVALID_OUTPUT_FILE 2

void generate_xml_parameter_node(XmlElement* parent, const AudioProcessorParameterGroup::AudioProcessorParameterNode* node)
{
    auto group = node->getGroup();
    if (group != nullptr) {
        auto childXml = parent->createNewChildElement("ports");
        childXml->setAttribute("name", group->getName());
        for (auto childPara : *group)
            generate_xml_parameter_node(childXml, childPara);
    } else {
        auto para = node->getParameter();
        auto childXml = parent->createNewChildElement("port");
        childXml->setAttribute("name", para->getName(1024));
        childXml->setAttribute("direction", "input"); // JUCE does not support output parameter.
        childXml->setAttribute("default", para->getDefaultValue());
        auto ranged = dynamic_cast<RangedAudioParameter*>(para);
        if (ranged) {
            auto range = ranged->getNormalisableRange();
            childXml->setAttribute("minimum", range.start);
            childXml->setAttribute("maximum", range.end);
        }
        childXml->setAttribute("content", "other");
    }
}

extern "C" {

__attribute__((used))
__attribute__((visibility("default")))
int generate_aap_metadata(const char *aapMetadataFullPath) {

    // FIXME: start application loop

    auto filter = createPluginFilter();
    String name = "aapports:juce/" + filter->getName();
    auto parameters = filter->getParameters();

    File aapMetadataFile{aapMetadataFullPath};
    if (!aapMetadataFile.getParentDirectory().exists()) {
        std::cerr << "Output directory '" << aapMetadataFile.getParentDirectory().getFullPathName()
                  << "' does not exist." << std::endl;
        return JUCEAAP_EXPORT_AAP_METADATA_INVALID_DIRECTORY;
    }
    std::unique_ptr<juce::XmlElement> pluginsElement{new XmlElement("plugins")};
    auto pluginElement = pluginsElement->createNewChildElement("plugin");
    pluginElement->setAttribute("name", JucePlugin_Name);
    pluginElement->setAttribute("category", JucePlugin_IsSynth ? "Synth" : "Effect");
    pluginElement->setAttribute("author", JucePlugin_Manufacturer);
    pluginElement->setAttribute("manufacturer", JucePlugin_ManufacturerWebsite);
    pluginElement->setAttribute("unique-id",
                                String::formatted("juceaap:%x", JucePlugin_PluginCode));
    //pluginElement->setAttribute("library", "lib" JucePlugin_Name ".so");
    pluginElement->setAttribute("library", "libjuce_jni.so");
    pluginElement->setAttribute("entrypoint", "GetJuceAAPFactory");
    pluginElement->setAttribute("assets", "");

    auto &tree = filter->getParameterTree();
    auto topLevelPortsElement = pluginElement->createNewChildElement("ports");
    for (auto node : tree) {
        generate_xml_parameter_node(topLevelPortsElement, node);
    }

    auto inChannels = filter->getBusesLayout().getMainInputChannelSet();
    for (int i = 0; i < inChannels.size(); i++) {
        auto portXml = topLevelPortsElement->createNewChildElement("port");
        portXml->setAttribute("direction", "input");
        portXml->setAttribute("content", "audio");
        portXml->setAttribute("name",
                              inChannels.getChannelTypeName(inChannels.getTypeOfChannel(i)));
    }
    auto outChannels = filter->getBusesLayout().getMainOutputChannelSet();
    for (int i = 0; i < outChannels.size(); i++) {
        auto portXml = topLevelPortsElement->createNewChildElement("port");
        portXml->setAttribute("direction", "output");
        portXml->setAttribute("content", "audio");
        portXml->setAttribute("name",
                              outChannels.getChannelTypeName(outChannels.getTypeOfChannel(i)));
    }
    if (filter->acceptsMidi()) {
        auto portXml = topLevelPortsElement->createNewChildElement("port");
        portXml->setAttribute("direction", "input");
        portXml->setAttribute("content", "midi");
        portXml->setAttribute("name", "MIDI input");
    }
    if (filter->producesMidi()) {
        auto portXml = topLevelPortsElement->createNewChildElement("port");
        portXml->setAttribute("direction", "output");
        portXml->setAttribute("content", "midi");
        portXml->setAttribute("name", "MIDI output");
    }

    FileOutputStream output{aapMetadataFile};
    if(!output.openedOk()) {
        std::cerr << "Cannot create output file '" << aapMetadataFile.getFullPathName() << "'" << std::endl;
        return JUCEAAP_EXPORT_AAP_METADATA_INVALID_OUTPUT_FILE;
    }
    auto s = pluginsElement->toString();
    output.writeText(s, false, false, "");
    output.flush();

    delete filter;

    return JUCEAAP_EXPORT_AAP_METADATA_SUCCESS;
}

}