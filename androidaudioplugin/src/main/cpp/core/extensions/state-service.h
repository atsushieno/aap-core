#ifndef AAP_CORE_STATE_SERVICE_H
#define AAP_CORE_STATE_SERVICE_H

#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/ext/state.h"
#include "aap/unstable/logging.h"
#include "aap/core/host/audio-plugin-host.h" // FIXME: we should eliminate dependency on types from this header.
#include "aap/core/aapxs/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

// implementation details
const int32_t OPCODE_GET_STATE_SIZE = 0;
const int32_t OPCODE_GET_STATE = 1;
const int32_t OPCODE_SET_STATE = 2;

const int32_t STATE_SHARED_MEMORY_SIZE = 0x100000; // 1M

class StatePluginClientExtension : public PluginClientExtensionImplBase {
    class Instance {
        friend class StatePluginClientExtension;

        aap_state_extension_t proxy{};

        static size_t internalGetStateSize(aap_state_extension_t *ext, AndroidAudioPlugin* plugin) {
            return ((Instance *) ext->aapxs_context)->getStateSize();
        }

        static void internalGetState(aap_state_extension_t *ext, AndroidAudioPlugin* plugin, aap_state_t *state) {
            return ((Instance *) ext->aapxs_context)->getState(state);
        }

        static void internalSetState(aap_state_extension_t *ext, AndroidAudioPlugin* plugin, aap_state_t *state) {
            return ((Instance *) ext->aapxs_context)->setState(state);
        }

        StatePluginClientExtension *owner;
        AAPXSClientInstance* aapxsInstance;

    public:
        Instance(StatePluginClientExtension *owner, AAPXSClientInstance *clientInstance)
                : owner(owner)
        {
            aapxsInstance = clientInstance;
        }

        void clientInvokePluginExtension(int32_t opcode) {
            owner->clientInvokePluginExtension(aapxsInstance, opcode);
        }

        size_t getStateSize() {
            clientInvokePluginExtension(OPCODE_GET_STATE_SIZE);
            return (size_t) *((int32_t *) aapxsInstance->data);
        }

        void getState(aap_state_t* result) {
            clientInvokePluginExtension(OPCODE_GET_STATE);
            result->data_size = (size_t) *((int32_t *) aapxsInstance->data);
            memcpy(result->data, (int32_t*) aapxsInstance->data + 1, result->data_size);
        }

        void setState(aap_state_t* source) {
            *((int32_t *) aapxsInstance->data) = (int32_t) source->data_size;
            memcpy((int32_t*) aapxsInstance->data + 1, source->data, source->data_size);
            clientInvokePluginExtension(OPCODE_SET_STATE);
        }

        AAPXSProxyContext asProxy() {
            proxy.aapxs_context = this;
            proxy.get_state_size = internalGetStateSize;
            proxy.get_state = internalGetState;
            proxy.set_state = internalSetState;
            return AAPXSProxyContext{aapxsInstance, this, &proxy};
        }
    };
private:

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define STATE_MAX_INSTANCE_COUNT 1024

    std::unique_ptr<Instance> instances[STATE_MAX_INSTANCE_COUNT]{};
    std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

public:
    StatePluginClientExtension()
            : PluginClientExtensionImplBase() {
    }

    AAPXSProxyContext asProxy(AAPXSClientInstance *clientInstance) override {
        size_t last = 0;
        for (; last < STATE_MAX_INSTANCE_COUNT; last++) {
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

class StatePluginServiceExtension : public PluginServiceExtensionImplBase {

    template<typename T>
    void withStateExtension(AndroidAudioPlugin* plugin, T defaultValue,
                            std::function<void(aap_state_extension_t *, AndroidAudioPlugin*)> func);

public:
    StatePluginServiceExtension()
            : PluginServiceExtensionImplBase(AAP_STATE_EXTENSION_URI) {
    }

    // invoked by AudioPluginService
    void onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override;
};


class StateExtensionFeature : public PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;

public:
    StateExtensionFeature()
            : PluginExtensionFeatureImpl(AAP_STATE_EXTENSION_URI, true, STATE_SHARED_MEMORY_SIZE),
              client(std::make_unique<StatePluginClientExtension>()),
              service(std::make_unique<StatePluginServiceExtension>()) {
    }

    PluginClientExtensionImplBase* getClient() { return client.get(); }
    PluginServiceExtensionImplBase* getService() { return service.get(); }
};

} // namespace aap


#endif //AAP_CORE_STATE_SERVICE_H
