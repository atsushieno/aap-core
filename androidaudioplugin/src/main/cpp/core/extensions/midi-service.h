#ifndef AAP_EXTENSIONS_MIDI2_SERVICE_H
#define AAP_EXTENSIONS_MIDI2_SERVICE_H


#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/aapxs.h"
#include "aap/ext/midi.h"
#include "aap/unstable/logging.h"
#include "aap/core/aapxs/extension-service.h"
#include "extension-service-impl.h"
#include "../AAPJniFacade.h"

namespace aap {

// implementation details
const int32_t OPCODE_GET_MAPPING_POLICY = 0;


class MidiPluginClientExtension : public PluginClientExtensionImplBase {
    class Instance {
        friend class MidiPluginClientExtension;

        aap_midi_extension_t proxy{};

        MidiPluginClientExtension *owner;
        AAPXSClientInstance* aapxsInstance;

        static enum aap_midi_mapping_policy internalGetMappingPolicy(aap_midi_extension_t* ext, AndroidAudioPlugin* plugin, const char* pluginId) {
            return ((Instance *) ext->aapxs_context)->getMappingPolicy(pluginId);
        }

    public:
        Instance(MidiPluginClientExtension *owner, AAPXSClientInstance *clientInstance)
                : owner(owner)
        {
            aapxsInstance = clientInstance;
        }

        enum aap_midi_mapping_policy getMappingPolicy(const char* pluginId) {
            auto len = strlen(pluginId);
            assert(len < AAP_MAX_PLUGIN_ID_SIZE);
            *((int32_t*) aapxsInstance->data) = len;
            strcpy((char*) ((int32_t*) aapxsInstance->data + 1), pluginId);
            ((char *)aapxsInstance->data) [sizeof(int32_t) + len] = 0;
            clientInvokePluginExtension(OPCODE_GET_MAPPING_POLICY, sizeof(int32_t) + len + 1);
            return *((enum aap_midi_mapping_policy *) aapxsInstance->data);
        }

        void clientInvokePluginExtension(int32_t opcode, int32_t messageSize) {
            owner->clientInvokePluginExtension(aapxsInstance, messageSize, opcode);
        }

        AAPXSProxyContext asProxy() {
            proxy.aapxs_context = this;
            proxy.get_mapping_policy = internalGetMappingPolicy;
            return AAPXSProxyContext{aapxsInstance, this, &proxy};
        }
    };

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define MIDI_AAPXS_MAX_INSTANCE_COUNT 1024

    std::unique_ptr<Instance> instances[MIDI_AAPXS_MAX_INSTANCE_COUNT]{};
    std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

public:
    MidiPluginClientExtension()
            : PluginClientExtensionImplBase() {
    }

    AAPXSProxyContext asProxy(AAPXSClientInstance *clientInstance) override {
        size_t last = 0;
        for (; last < MIDI_AAPXS_MAX_INSTANCE_COUNT; last++) {
            if (instances[last] == nullptr)
                break;
            if (instances[last]->aapxsInstance == clientInstance)
                return instances[instance_map[clientInstance->plugin_instance_id]]->asProxy();
        }
        instances[last] = std::make_unique<Instance>(this, clientInstance);
        instance_map[clientInstance->plugin_instance_id] = (int32_t) last;
        return instances[last]->asProxy();
    }
};

class MidiPluginServiceExtension : public PluginServiceExtensionImplBase {

public:
    MidiPluginServiceExtension()
            : PluginServiceExtensionImplBase(AAP_MIDI_EXTENSION_URI) {
    }

    // invoked by AudioPluginService
    void onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override;
};


class MidiExtensionFeature : public PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;

public:
    MidiExtensionFeature()
            : PluginExtensionFeatureImpl(AAP_MIDI_EXTENSION_URI, false, sizeof(aap_midi_extension_t)),
              client(std::make_unique<MidiPluginClientExtension>()),
              service(std::make_unique<MidiPluginServiceExtension>()) {
    }

    PluginClientExtensionImplBase* getClient() { return client.get(); }
    PluginServiceExtensionImplBase* getService() { return service.get(); }
};

} // namespace aap

#endif //AAP_EXTENSIONS_MIDI2_SERVICE_H
