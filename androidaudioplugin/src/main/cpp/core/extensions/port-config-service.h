#ifndef AAP_CORE_PORT_CONFIG_SERVICE_H
#define AAP_CORE_PORT_CONFIG_SERVICE_H

#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/ext/port-config.h"
#include "aap/unstable/logging.h"
#include "aap/core/host/audio-plugin-host.h" // FIXME: we should eliminate dependency on types from this header.
#include "aap/core/aapxs/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

// implementation details
const int32_t OPCODE_PORT_CONFIG_GET_OPTIONS = 0;
const int32_t OPCODE_PORT_CONFIG_SELECT = 1;

const int32_t PORT_CONFIG_SHARED_MEMORY_SIZE = 0x1000; // 65536

class PortConfigPluginClientExtension : public PluginClientExtensionImplBase {
    class Instance {
        friend class PortConfigPluginClientExtension;

        aap_port_config_extension_t proxy{};

        static void internalGetOptions(AndroidAudioPluginExtensionTarget target, aap_port_config_t *destination) {
            ((Instance *) target.aapxs_context)->getOptions(destination);
        }

        static void internalSelect(AndroidAudioPluginExtensionTarget target, int32_t configIndex) {
            ((Instance *) target.aapxs_context)->select(configIndex);
        }

        PortConfigPluginClientExtension *owner;
        AAPXSClientInstance* aapxsInstance;

    public:
        Instance(PortConfigPluginClientExtension *owner, AAPXSClientInstance *clientInstance)
                : owner(owner)
        {
            aapxsInstance = clientInstance;
        }

        void clientInvokePluginExtension(int32_t opcode) {
            owner->clientInvokePluginExtension(aapxsInstance, opcode);
        }

        void getOptions(aap_port_config_t* destination) {
            clientInvokePluginExtension(OPCODE_PORT_CONFIG_GET_OPTIONS);
            destination->data_size = (size_t) *((int32_t *) aapxsInstance->data);
            memcpy(destination->data, (int32_t*) aapxsInstance->data + 1, destination->data_size);
        }

        void select(int32_t configIndex) {
            *((int32_t *) aapxsInstance->data) = configIndex;
            clientInvokePluginExtension(OPCODE_PORT_CONFIG_SELECT);
        }

        AAPXSProxyContext asProxy() {
            proxy.get_options = internalGetOptions;
            proxy.select = internalSelect;
            return AAPXSProxyContext{aapxsInstance, this, &proxy};
        }
    };

private:

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define PORT_CONFIG_MAX_INSTANCE_COUNT 1024

    std::unique_ptr<Instance> instances[PORT_CONFIG_MAX_INSTANCE_COUNT]{};
    std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

public:
    PortConfigPluginClientExtension()
            : PluginClientExtensionImplBase() {
    }

    AAPXSProxyContext asProxy(AAPXSClientInstance *clientInstance) override {
        size_t last = 0;
        for (; last < PORT_CONFIG_MAX_INSTANCE_COUNT; last++) {
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

class PortConfigPluginServiceExtension : public PluginServiceExtensionImplBase {

    template<typename T>
    void withPortConfigExtension(AndroidAudioPlugin* plugin, T defaultValue,
                            std::function<void(aap_port_config_extension_t *,
                                               AndroidAudioPluginExtensionTarget)> func);

public:
    PortConfigPluginServiceExtension()
            : PluginServiceExtensionImplBase(AAP_PORT_CONFIG_EXTENSION_URI) {
    }

    // invoked by AudioPluginService
    void onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override;
};


class PortConfigExtensionFeature : public PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;

public:
    PortConfigExtensionFeature()
            : PluginExtensionFeatureImpl(AAP_PORT_CONFIG_EXTENSION_URI, PORT_CONFIG_SHARED_MEMORY_SIZE),
              client(std::make_unique<PortConfigPluginClientExtension>()),
              service(std::make_unique<PortConfigPluginServiceExtension>()) {
    }

    PluginClientExtensionImplBase* getClient() { return client.get(); }
    PluginServiceExtensionImplBase* getService() { return service.get(); }
};

} // namespace aap


#endif //AAP_CORE_PORT_CONFIG_SERVICE_H
