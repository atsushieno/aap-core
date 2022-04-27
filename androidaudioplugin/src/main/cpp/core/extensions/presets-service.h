#ifndef AAP_CORE_PRESETS_SERVICE_H
#define AAP_CORE_PRESETS_SERVICE_H

#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/unstable/presets.h"
#include "aap/unstable/logging.h"
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

// implementation details
const int32_t OPCODE_GET_PRESET_COUNT = 0;
const int32_t OPCODE_GET_PRESET_DATA_SIZE = 1;
const int32_t OPCODE_GET_PRESET_DATA = 2;
const int32_t OPCODE_GET_PRESET_INDEX = 3;
const int32_t OPCODE_SET_PRESET_INDEX = 4;

const int32_t PRESETS_SHARED_MEMORY_SIZE = 0x100000; // 1M

class PresetsPluginClientExtension : public PluginClientExtensionImplBase {
    class Instance {
        friend class PresetsPluginClientExtension;

        aap_presets_extension_t proxy{};

        static int32_t internalGetPresetCount(aap_presets_context_t *context) {
            return ((Instance *) context->context)->getPresetCount();
        }

        static int32_t internalGetPresetDataSize(aap_presets_context_t *context, int32_t index) {
            return ((Instance *) context->context)->getPresetDataSize(index);
        }

        static void internalGetPreset(aap_presets_context_t *context, int32_t index, bool skipBinary,
                                      aap_preset_t *preset) {
            ((Instance *) context->context)->getPreset(index, skipBinary,
                                                                           preset);
        }

        static int32_t internalGetPresetIndex(aap_presets_context_t *context) {
            return ((Instance *) context->context)->getPresetIndex();
        }

        static void internalSetPresetIndex(aap_presets_context_t *context, int32_t index) {
            return ((Instance *) context->context)->setPresetIndex(index);
        }

        PresetsPluginClientExtension *owner;
        AAPXSClientInstance* aapxsInstance;

    public:
        Instance(PresetsPluginClientExtension *owner, AAPXSClientInstance *clientInstance)
            : owner(owner)
        {
            aapxsInstance = clientInstance;
        }

        void clientInvokePluginExtension(int32_t opcode) {
            owner->clientInvokePluginExtension(aapxsInstance, opcode);
        }

        int32_t getPresetCount() {
            clientInvokePluginExtension(OPCODE_GET_PRESET_COUNT);
            return *((int32_t *) aapxsInstance->data);
        }

        int32_t getPresetDataSize(int32_t index) {
            *((int32_t *) aapxsInstance->data) = index;
            clientInvokePluginExtension(OPCODE_GET_PRESET_DATA_SIZE);
            return *((int32_t *) aapxsInstance->data);
        }

        void getPreset(int32_t index, bool skipBinary, aap_preset_t *result) {
            *((int32_t *) aapxsInstance->data) = index;
            *(bool *) ((int32_t *) aapxsInstance->data + 1) = skipBinary;
            clientInvokePluginExtension(OPCODE_GET_PRESET_DATA);
            result->data_size = *((int32_t *) aapxsInstance->data);
            strncpy(result->name, (const char *) ((int32_t *) aapxsInstance->data + 1), 256);
            memcpy(result->data, ((int32_t *) aapxsInstance->data + 1), result->data_size);
        }

        int32_t getPresetIndex() {
            clientInvokePluginExtension(OPCODE_GET_PRESET_INDEX);
            return *((int32_t *) aapxsInstance->data);
        }

        void setPresetIndex(int32_t index) {
            *((int32_t *) aapxsInstance->data) = index;
            clientInvokePluginExtension(OPCODE_SET_PRESET_INDEX);
        }

        void *asProxy() {
            proxy.context = this;
            proxy.get_preset_count = internalGetPresetCount;
            proxy.get_preset_data_size = internalGetPresetDataSize;
            proxy.get_preset = internalGetPreset;
            proxy.get_preset_index = internalGetPresetIndex;
            proxy.set_preset_index = internalSetPresetIndex;
            return &proxy;
        }
    };

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define PRESETS_MAX_INSTANCE_COUNT 1024

    std::unique_ptr<Instance> instances[PRESETS_MAX_INSTANCE_COUNT]{};
    std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

public:
    PresetsPluginClientExtension()
            : PluginClientExtensionImplBase() {
    }

    void *asProxy(AAPXSClientInstance *clientInstance) override {
        size_t last = 0;
        for (; last < PRESETS_MAX_INSTANCE_COUNT; last++) {
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

class PresetsPluginServiceExtension : public PluginServiceExtensionImplBase {

    template<typename T>
    void withPresetExtension(aap::LocalPluginInstance *instance, T defaultValue,
                             std::function<void(aap_presets_extension_t *,
                                                aap_presets_context_t *)> func);

public:
    PresetsPluginServiceExtension()
        : PluginServiceExtensionImplBase(AAP_PRESETS_EXTENSION_URI) {
    }

    // invoked by AudioPluginService
    void onInvoked(void* contextInstance, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override;
};


class PresetsExtensionFeature : public PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;

public:
    PresetsExtensionFeature()
        : PluginExtensionFeatureImpl(AAP_PRESETS_EXTENSION_URI, PRESETS_SHARED_MEMORY_SIZE),
          client(std::make_unique<PresetsPluginClientExtension>()),
          service(std::make_unique<PresetsPluginServiceExtension>()) {
    }

    PluginClientExtensionImplBase* getClient() { return client.get(); }
    PluginServiceExtensionImplBase* getService() { return service.get(); }
};

} // namespace aap


#endif //AAP_CORE_PRESETS_SERVICE_H
