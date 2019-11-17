#include <unistd.h>
#include <dlfcn.h>
#include <cmath>
#include <cstring>
#include <vector>

#include "aap/android-audio-plugin-host.hpp"

extern aap::PluginInformation **local_plugin_infos;

namespace aaplv2sample {

// In this implementation we have fixed buffers and everything is simplified
typedef struct {
    aap::PluginInstance *plugin;
    AndroidAudioPluginBuffer *plugin_buffer;
} AAPInstanceUse;

int runHostAAP(int sampleRate, const char **pluginIDs, int numPluginIDs, void *wavL, void *wavR, int wavLength,
               void *outWavL, void *outWavR) {
    auto host = new aap::PluginHost(local_plugin_infos);

    int buffer_size = 44100 * sizeof(float);
    int float_count = buffer_size / sizeof(float);

    std::vector<AAPInstanceUse *> instances;

    /* instantiate plugins and connect ports */

    float *audioInL = (float *) calloc(buffer_size, 1);
    float *audioInR = (float *) calloc(buffer_size, 1);
    float *midiIn = (float *) calloc(buffer_size, 1);
    float *controlIn = (float *) calloc(buffer_size, 1);
    float *dummyBuffer = (float *) calloc(buffer_size, 1);

    float *currentAudioInL = audioInL, *currentAudioInR = audioInR, *currentAudioOutL = nullptr, *currentAudioOutR = nullptr, *currentMidiIn = midiIn, *currentMidiOut = nullptr;

    // FIXME: pluginIDs should be enough (but iteration by it crashes so far)
    for (int i = 0; i < numPluginIDs; i++) {
        auto instance = host->instantiatePlugin(pluginIDs[i]);
        if (instance == nullptr) {
            // FIXME: the entire code needs review to eliminate those printf/puts/stdout/stderr uses.
            printf("plugin %s failed to instantiate. Skipping.\n", pluginIDs[i]);
            continue;
        }
        AAPInstanceUse *iu = new AAPInstanceUse();
        auto desc = instance->getPluginDescriptor();
        iu->plugin = instance;
        iu->plugin_buffer = new AndroidAudioPluginBuffer();
        iu->plugin_buffer->num_frames = buffer_size / sizeof(float);
        int nPorts = desc->getNumPorts();
        iu->plugin_buffer->buffers = (void **) calloc(nPorts + 1, sizeof(void *));
        int numAudioIn = 0, numAudioOut = 0;
        for (int p = 0; p < nPorts; p++) {
            auto port = desc->getPort(p);
            if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
                iu->plugin_buffer->buffers[p] = numAudioIn++ > 0 ? currentAudioInR : currentAudioInL;
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                     port->getContentType() == aap::AAP_CONTENT_TYPE_AUDIO)
                iu->plugin_buffer->buffers[p] = (numAudioOut++ > 0 ? currentAudioOutR : currentAudioOutL) = (float *) calloc(buffer_size, 1);
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT &&
                     port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
                iu->plugin_buffer->buffers[p] = currentMidiIn;
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_OUTPUT &&
                     port->getContentType() == aap::AAP_CONTENT_TYPE_MIDI)
                iu->plugin_buffer->buffers[p] = currentMidiOut = (float *) calloc(buffer_size, 1);
            else if (port->getPortDirection() == aap::AAP_PORT_DIRECTION_INPUT)
                iu->plugin_buffer->buffers[p] = controlIn;
            else
                iu->plugin_buffer->buffers[p] = dummyBuffer;
        }
        instances.push_back(iu);
        if (currentAudioOutL)
            currentAudioInL = currentAudioOutL;
        if (currentAudioOutR)
            currentAudioInR = currentAudioOutR;
        if (currentMidiOut)
            currentMidiIn = currentMidiOut;
    }

    assert(instances.size() > 0);

    // prepare connections
    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances[i];
        auto plugin = iu->plugin;
        plugin->prepare(sampleRate, false, iu->plugin_buffer);
    }

    // prepare inputs - dummy
    for (int i = 0; i < float_count; i++)
        controlIn[i] = 0.5;

    // activate, run, deactivate
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances[i];
        instance->plugin->activate();
    }
    for (int b = 0; b < wavLength; b += buffer_size) {
        // prepare inputs -audioIn
        memcpy(audioInL, ((char*) wavL) + b, b + buffer_size < wavLength ? buffer_size : wavLength - b);
        memcpy(audioInR, ((char*) wavR) + b, b + buffer_size < wavLength ? buffer_size : wavLength - b);
        // process ports
        for (int i = 0; i < instances.size(); i++) {
            auto instance = instances[i];
            instance->plugin->process(instance->plugin_buffer, 0);
        }
        memcpy(((char*) outWavL) + b, currentAudioOutL, b + buffer_size < wavLength ? buffer_size : wavLength - b);
        memcpy(((char*) outWavR) + b, currentAudioOutR, b + buffer_size < wavLength ? buffer_size : wavLength - b);
    }
    for (int i = 0; i < instances.size(); i++) {
        auto instance = instances[i];
        instance->plugin->deactivate();
    }


    for (int i = 0; i < instances.size(); i++) {
        auto iu = instances[i];
        for (int p = 0; iu->plugin_buffer->buffers[p]; p++) {
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
    }

    delete host;

    free(audioInL);
    free(audioInR);
    free(midiIn);
    free(controlIn);
    free(dummyBuffer);

    return 0;
}
} // namesoace aaplv2
