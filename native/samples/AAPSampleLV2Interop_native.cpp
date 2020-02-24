#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "aap/android-audio-plugin-host.hpp"

namespace aaplv2sample {

// In this implementation we have fixed buffers and everything is simplified
typedef struct {
    aap::PluginInstance *plugin;
    AndroidAudioPluginBuffer *plugin_buffer;
} AAPInstanceUse;

int runHostAAP(int sampleRate, const char *pluginID, void *wavL, void *wavR, int wavLength,
               void *outWavL, void *outWavR, float* parameters) {
    auto host = new aap::PluginHost();

    int buffer_size = 44100 * sizeof(float);
    int float_count = buffer_size / sizeof(float);

    /* instantiate plugins and connect ports */

    float *audioInL = (float *) calloc(buffer_size, 1);
    float *audioInR = (float *) calloc(buffer_size, 1);
    float *midiIn = (float *) calloc(buffer_size, 1);
    float *controlIn = (float *) calloc(buffer_size, 1);
    float *dummyBuffer = (float *) calloc(buffer_size, 1);

    float *currentAudioInL = audioInL, *currentAudioInR = audioInR, *currentAudioOutL = nullptr, *currentAudioOutR = nullptr, *currentMidiIn = midiIn, *currentMidiOut = nullptr;

    auto instance = host->instantiatePlugin(pluginID);
    if (instance == nullptr) {
        // FIXME: the entire code needs review to eliminate those printf/puts/stdout/stderr uses.
        printf("plugin %s failed to instantiate.\n", pluginID);
        return -1; // FIXME: determine error code
    }
    AAPInstanceUse *iu = new AAPInstanceUse();
    auto desc = instance->getPluginDescriptor();
    iu->plugin = instance;
    iu->plugin_buffer = new AndroidAudioPluginBuffer();
    iu->plugin_buffer->num_frames = buffer_size / sizeof(float);
    int nPorts = desc->getNumPorts();
    iu->plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
    int numAudioIn = 0, numAudioOut = 0;
    int midiInPort = -1;
    for (int p = 0; p < nPorts; p++) {
        auto port = desc->getPort(p);
        if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
            port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
            iu->plugin_buffer->buffers[p] = numAudioIn++ > 0 ? currentAudioInR : currentAudioInL;
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
            iu->plugin_buffer->buffers[p] = (numAudioOut++ > 0 ? currentAudioOutR : currentAudioOutL) = (float *) calloc(buffer_size, 1);
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI) {
			iu->plugin_buffer->buffers[p] = currentMidiIn;
			midiInPort = p;
		} else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                 port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
            iu->plugin_buffer->buffers[p] = currentMidiOut = (float *) calloc(buffer_size, 1);
        else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
            iu->plugin_buffer->buffers[p] = controlIn;
        else
            iu->plugin_buffer->buffers[p] = dummyBuffer;
    }

    // prepare connections
    instance->prepare(sampleRate, false, iu->plugin_buffer);

    // prepare inputs - dummy
    for (int i = 0; i < instance->getPluginDescriptor()->getNumPorts(); i++) {
		if (i == midiInPort) {
            auto mb = (uint8_t*) currentMidiIn;
            int length = 20;
            int16_t fps = -30;
            int16_t ticksPerFrame = 100;
            *((int*) mb) = 0xF000 | ticksPerFrame | (fps << 8); // 30fps, 100 ticks per frame
            //*(int*) mb = 480;
            *((int*) mb + 1) = length;
            mb[8] = 0; // program change
            mb[9] = 0xC0;
            mb[10] = 0;
            mb[11] = 0; // note on
            mb[12] = 0x90;
            mb[13] = 0x39;
            mb[14] = 0x70;
            mb[15] = 0; // note on
            mb[16] = 0x90;
            mb[17] = 0x3D;
            mb[18] = 0x70;
            mb[19] = 0; // note on
            mb[20] = 0x90;
            mb[21] = 0x40;
            mb[22] = 0x70;
            mb[23] = 0x80; // note off
            mb[24] = 0x70;
            mb[25] = 0x80;
            mb[26] = 0x45;
            mb[27] = 0x00;
		}
		else
            for (int b = 0; b < float_count; b++)
                    controlIn[b] = parameters != nullptr ? parameters[i] : instance->getPluginDescriptor()->getPort(i)->getDefaultValue();
	}
    // activate, run, deactivate
    instance->activate();

    for (int b = 0; b < wavLength; b += buffer_size) {
        // prepare inputs -audioIn
		int size = b + buffer_size < wavLength ? buffer_size : wavLength - b;
        memcpy(audioInL, ((char*) wavL) + b, size);
        memcpy(audioInR, ((char*) wavR) + b, size);
        // process ports
        instance->process(iu->plugin_buffer, 0);
        memcpy(((char*) outWavL) + b, currentAudioOutL, b + buffer_size < wavLength ? buffer_size : wavLength - b);
        memcpy(((char*) outWavR) + b, currentAudioOutR, b + buffer_size < wavLength ? buffer_size : wavLength - b);
    }

    instance->deactivate();

    for (int p = 0; p < iu->plugin_buffer->num_buffers; p++) {
        if(iu->plugin_buffer->buffers[p] != nullptr
            && iu->plugin_buffer->buffers[p] != dummyBuffer
            && iu->plugin_buffer->buffers[p] != audioInL
            && iu->plugin_buffer->buffers[p] != audioInR
            && iu->plugin_buffer->buffers[p] != midiIn
            && iu->plugin_buffer->buffers[p] != controlIn)
            free(iu->plugin_buffer->buffers[p]);
    }
    free(iu->plugin_buffer->buffers);
    delete iu->plugin_buffer;
    delete iu->plugin;
    delete iu;

    delete host;

    free(audioInL);
    free(audioInR);
    free(midiIn);
    free(controlIn);
    free(dummyBuffer);

    return 0;
}
} // namesoace aaplv2
