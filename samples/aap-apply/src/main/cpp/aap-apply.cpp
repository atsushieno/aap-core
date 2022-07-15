
#include "aap/core/host/audio-plugin-host.h"
#include "LabSound/LabSound.h"

namespace aapapply {

    class PluginInputSource {
    public:
        virtual bool isActive() = 0;
        virtual bool fillNext(aap::PluginBuffer *buffer) = 0;
    };

    /*
    class PluginInputSource : public PluginInputSource {
    public:
        bool isActive() override;
        bool fillNext(PluginBuffer *buffer) override;
    };
    */

    class AAPApply;

    class AudioPluginClientApp {
        AAPApply* parent;
        const double recordTimeMilliseconds = 10000;
        std::unique_ptr<lab::AudioContext> audioContext;

    public:
        [[maybe_unused]] AudioPluginClientApp(AAPApply* parent, int totalSamples)
            : parent(parent),
              recordTimeMilliseconds(totalSamples / 44100.0 * 1000.0) {
            lab::AudioStreamConfig configIn = lab::GetDefaultInputAudioDeviceConfiguration();
            audioContext = lab::MakeOfflineAudioContext(configIn, recordTimeMilliseconds);
        }

    };

    class AAPApply {
        PluginInputSource *source;
        aap::PluginClient *client;
        aap::RemotePluginInstance *instance;

    public:
        AAPApply(PluginInputSource* source, aap::PluginClient* client, aap::RemotePluginInstance* instance)
            : source(source), client(client), instance(instance) {
        }

        void enable() {
        }

        void disable() {
        }
    };

} // namespace aap

