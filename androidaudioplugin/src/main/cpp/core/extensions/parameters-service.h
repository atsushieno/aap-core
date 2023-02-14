#ifndef AAP_CORE_PARAMETERS_SERVICE_H
#define AAP_CORE_PARAMETERS_SERVICE_H


#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/ext/parameters.h"
#include "aap/unstable/logging.h"
#include "aap/core/aapxs/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

class ParametersPluginClientExtension : public PluginClientExtensionImplBase {
    class Instance {
        friend class ParametersPluginClientExtension;

        aap_parameters_extension_t proxy{};

        ParametersPluginClientExtension *owner;
        AAPXSClientInstance* aapxsInstance;

    public:
        Instance(ParametersPluginClientExtension *owner, AAPXSClientInstance *clientInstance)
            : owner(owner)
        {
            aapxsInstance = clientInstance;
        }

        void clientInvokePluginExtension(int32_t opcode) {
            owner->clientInvokePluginExtension(aapxsInstance, opcode);
        }

        AAPXSProxyContext asProxy() {
            return AAPXSProxyContext{aapxsInstance, this, &proxy};
        }
    };

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define PARAMETERS_AAPXS_MAX_INSTANCE_COUNT 1024

    std::unique_ptr<Instance> instances[PARAMETERS_AAPXS_MAX_INSTANCE_COUNT]{};
    std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

public:
    ParametersPluginClientExtension()
            : PluginClientExtensionImplBase() {
    }

    AAPXSProxyContext asProxy(AAPXSClientInstance *clientInstance) override {
        size_t last = 0;
        for (; last < PARAMETERS_AAPXS_MAX_INSTANCE_COUNT; last++) {
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

class ParametersPluginServiceExtension : public PluginServiceExtensionImplBase {

public:
    ParametersPluginServiceExtension()
            : PluginServiceExtensionImplBase(AAP_PARAMETERS_EXTENSION_URI) {
    }

    // invoked by AudioPluginService
    void onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override {
        assert(false); // should not happen
    }
};


class ParametersExtensionFeature : public PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;

public:
    ParametersExtensionFeature()
            : PluginExtensionFeatureImpl(AAP_PARAMETERS_EXTENSION_URI, false, sizeof(aap_parameters_extension_t)),
              client(std::make_unique<ParametersPluginClientExtension>()),
              service(std::make_unique<ParametersPluginServiceExtension>()) {
    }

    PluginClientExtensionImplBase* getClient() { return client.get(); }
    PluginServiceExtensionImplBase* getService() { return service.get(); }
};

} // namespace aap



#endif //AAP_CORE_PARAMETERS_SERVICE_H
