#ifndef AAP_EXTENSIONS_MIDI2_SERVICE_H
#define AAP_EXTENSIONS_MIDI2_SERVICE_H


#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/ext/aap-midi2.h"
#include "aap/unstable/logging.h"
#include "aap/core/aapxs/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

// implementation details

class Midi2PluginClientExtension : public PluginClientExtensionImplBase {
    class Instance {
        friend class Midi2PluginClientExtension;

        aap_midi2_extension_t proxy{};

        //Midi2PluginClientExtension *owner;
        AAPXSClientInstance* aapxsInstance;

    public:
        Instance(Midi2PluginClientExtension *owner, AAPXSClientInstance *clientInstance)
                //: owner(owner)
        {
            aapxsInstance = clientInstance;
        }

        /*
        void clientInvokePluginExtension(int32_t opcode) {
            owner->clientInvokePluginExtension(aapxsInstance, opcode);
        }
        */

        AAPXSProxyContext asProxy() {
            proxy.context = this;
            return AAPXSProxyContext{aapxsInstance, nullptr, &proxy};
        }
    };

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define MIDI2_AAPXS_MAX_INSTANCE_COUNT 1024

    std::unique_ptr<Instance> instances[MIDI2_AAPXS_MAX_INSTANCE_COUNT]{};
    std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

public:
    Midi2PluginClientExtension()
            : PluginClientExtensionImplBase() {
    }

    AAPXSProxyContext asProxy(AAPXSClientInstance *clientInstance) override {
        size_t last = 0;
        for (; last < MIDI2_AAPXS_MAX_INSTANCE_COUNT; last++) {
            if (instances[last] == nullptr)
                break;
            if (instances[last]->aapxsInstance == clientInstance)
                return instances[instance_map[last]]->asProxy();
        }
        instances[last] = std::make_unique<Instance>(this, clientInstance);
        instance_map[clientInstance->plugin_instance_id] = (int32_t) last;
        return instances[last]->asProxy();
    }
};

class Midi2PluginServiceExtension : public PluginServiceExtensionImplBase {

public:
    Midi2PluginServiceExtension()
            : PluginServiceExtensionImplBase(AAP_MIDI2_EXTENSION_URI) {
    }

    // invoked by AudioPluginService
    void onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override {
        assert(false); // should not happen
    }
};


class Midi2ExtensionFeature : public PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;

public:
    Midi2ExtensionFeature()
            : PluginExtensionFeatureImpl(AAP_MIDI2_EXTENSION_URI, sizeof(aap_midi2_extension_t)),
              client(std::make_unique<Midi2PluginClientExtension>()),
              service(std::make_unique<Midi2PluginServiceExtension>()) {
    }

    PluginClientExtensionImplBase* getClient() { return client.get(); }
    PluginServiceExtensionImplBase* getService() { return service.get(); }
};

} // namespace aap

#endif //AAP_EXTENSIONS_MIDI2_SERVICE_H
