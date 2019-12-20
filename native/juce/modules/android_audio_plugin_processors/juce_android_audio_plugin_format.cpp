
#include "juce_android_audio_plugin_format.h"

using namespace aap;
using namespace juce;

namespace juceaap {

JuceAndroidAudioPluginEditor::JuceAndroidAudioPluginEditor(AudioProcessor *processor, aap::EditorInstance *native)
    : juce::AudioProcessorEditor(processor), native(native)
{
}

double JuceAndroidAudioPluginInstance::getTailLengthSeconds() const {
    return native->getTailTimeInMilliseconds() / 1000.0;
}

static void fillPluginDescriptionFromNative(PluginDescription &description,
                                            const aap::PluginInformation &src) {
    description.name = src.getName();
    description.pluginFormatName = "AAP";

    description.category.clear();
    description.category += juce::String(src.getPrimaryCategory());

    description.manufacturerName = src.getManufacturerName();
    description.version = src.getVersion();
    description.fileOrIdentifier = src.getIdentifier();
    // So far this is as hacky as AudioUnit implementation.
    description.lastFileModTime = Time();
    description.lastInfoUpdateTime = Time(src.getLastInfoUpdateTime());
    description.uid = String(src.getPluginID()).hashCode();
    description.isInstrument = src.isInstrument();
    for (int i = 0; i < src.getNumPorts(); i++) {
        auto port = src.getPort(i);
        auto dir = port->getPortDirection();
        if (dir == AAP_PORT_DIRECTION_INPUT)
            description.numInputChannels++;
        else if (dir == AAP_PORT_DIRECTION_OUTPUT)
            description.numOutputChannels++;
    }
    description.hasSharedContainer = src.hasSharedContainer();
}


void JuceAndroidAudioPluginInstance::fillNativeAudioBuffers(AndroidAudioPluginBuffer *dst,
                                                            AudioBuffer<float> &buffer) {
    int n = buffer.getNumChannels();
    for (int i = 0; i < n; i++)
        dst->buffers[i] = (void *) buffer.getReadPointer(i);
}

void JuceAndroidAudioPluginInstance::fillNativeMidiBuffers(AndroidAudioPluginBuffer *dst,
                                                           MidiBuffer &buffer, int bufferIndex) {
    dst->buffers[bufferIndex] = (void *) buffer.data.getRawDataPointer();
}

int JuceAndroidAudioPluginInstance::getNumBuffers(AndroidAudioPluginBuffer *buffer) {
    auto b = buffer->buffers;
    int n = 0;
    while (b) {
        n++;
        b++;
    }
    return n;
}

JuceAndroidAudioPluginInstance::JuceAndroidAudioPluginInstance(aap::PluginInstance *nativePlugin)
        : native(nativePlugin),
          sample_rate(-1) {
}

void
JuceAndroidAudioPluginInstance::prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) {
    sample_rate = sampleRate;

    // minimum setup, as the pointers are not passed by JUCE framework side.
    int n = native->getPluginDescriptor()->getNumPorts();
    buffer.buffers = new void *[n];
    // FIXME: get audio channel count and replace "2"
    dummy_raw_buffer = (float *) calloc(maximumExpectedSamplesPerBlock * sizeof(float) * 2,
                                        1);
    for (int i = 0; i < n; i++)
        buffer.buffers[i] = dummy_raw_buffer;

    native->prepare(sampleRate, maximumExpectedSamplesPerBlock, &buffer);

    native->activate();
}

void JuceAndroidAudioPluginInstance::releaseResources() {
    native->dispose();

    if (buffer.buffers) {
        delete buffer.buffers;
        buffer.buffers = NULL;
    }

    if (dummy_raw_buffer) {
        free(dummy_raw_buffer);
        dummy_raw_buffer = NULL;
    }
}

void JuceAndroidAudioPluginInstance::processBlock(AudioBuffer<float> &audioBuffer,
                                                  MidiBuffer &midiMessages) {
    fillNativeAudioBuffers(&buffer, audioBuffer);
    fillNativeMidiBuffers(&buffer, midiMessages, audioBuffer.getNumChannels());
    native->process(&buffer, 0);
}

bool JuceAndroidAudioPluginInstance::hasMidiPort(bool isInput) const {
    auto d = native->getPluginDescriptor();
    for (int i = 0; i < d->getNumPorts(); i++) {
        auto p = d->getPort(i);
        if (p->getPortDirection() ==
            (isInput ? AAP_PORT_DIRECTION_INPUT : AAP_PORT_DIRECTION_OUTPUT) &&
            p->getContentType() == AAP_CONTENT_TYPE_MIDI)
            return true;
    }
    return false;
}

AudioProcessorEditor *JuceAndroidAudioPluginInstance::createEditor() {
    if (!native->getPluginDescriptor()->hasEditor())
        return nullptr;
    auto ret = new JuceAndroidAudioPluginEditor(this, native->createEditor());
    ret->startEditorUI();
    return ret;
}

void JuceAndroidAudioPluginInstance::fillInPluginDescription(PluginDescription &description) const {
    auto src = native->getPluginDescriptor();
    fillPluginDescriptionFromNative(description, *src);
}

const aap::PluginInformation *
JuceAndroidAudioPluginFormat::findDescriptorFrom(const PluginDescription &desc) {
    auto it = cached_descs.begin();
    while (it.next())
        if (it.getValue()->uid == desc.uid)
            return it.getKey();
    return NULL;
}

JuceAndroidAudioPluginFormat::JuceAndroidAudioPluginFormat(
        const aap::PluginInformation *const *pluginDescriptors)
        : android_host(aap::PluginHost(pluginDescriptors)) {
    for (int i = 0; i < android_host.getNumPluginDescriptors(); i++) {
        auto d = android_host.getPluginDescriptorAt(i);
        auto dst = new PluginDescription();
        fillPluginDescriptionFromNative(*dst, *d);
        cached_descs.set(d, dst);
    }
}

JuceAndroidAudioPluginFormat::~JuceAndroidAudioPluginFormat() {
    auto it = cached_descs.begin();
    while (it.next())
        delete it.getValue();
    cached_descs.clear();
}

void JuceAndroidAudioPluginFormat::findAllTypesForFile(OwnedArray <PluginDescription> &results,
                                                       const String &fileOrIdentifier) {
    auto id = fileOrIdentifier.toRawUTF8();
    // FIXME: everything is already cached, so this can be simplified.
    for (int i = 0; i < android_host.getNumPluginDescriptors(); i++) {
        auto d = android_host.getPluginDescriptorAt(i);
        if (strcmp(id, d->getName().data()) == 0 ||
            strcmp(id, d->getIdentifier().data()) == 0) {
            auto dst = cached_descs[d];
            if (!dst)
                // doesn't JUCE handle invalid fileOrIdentifier?
                return;
            results.add(dst);
        }
    }
}

void JuceAndroidAudioPluginFormat::createPluginInstance(const PluginDescription &description,
                                                        double initialSampleRate,
                                                        int initialBufferSize,
                                                        PluginCreationCallback callback) {
    auto descriptor = findDescriptorFrom(description);
    String error("");
    if (descriptor == nullptr) {
        error << "Android Audio Plugin " << description.name << "was not found.";
        callback(nullptr, error);
    } else {
        auto androidInstance = android_host.instantiatePlugin(
                descriptor->getIdentifier().data());
        std::unique_ptr <JuceAndroidAudioPluginInstance> instance{
                new JuceAndroidAudioPluginInstance(androidInstance)};
        callback(std::move(instance), error);
    }
}
} // namespace
