#ifndef AAP_CORE_STANDARD_EXTENSIONS_IMPL_H
#define AAP_CORE_STANDARD_EXTENSIONS_IMPL_H

#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/unstable/presets.h"
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

class PresetsPluginClientExtension : public PluginClientExtensionImpl {
class Instance {
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

public:
    PresetsPluginClientExtension()
            : PluginClientExtensionImpl() {
    }

    void *asProxy(AAPXSClientInstance *clientInstance) override {
        // FIXME: we must retain reference to this Instance.
        return Instance(this, clientInstance).asProxy();
    }
};

class PresetsPluginServiceExtension : public PluginServiceExtensionImpl {

    template<typename T>
    void withPresetExtension(aap::LocalPluginInstance *instance, T defaultValue,
                             std::function<void(aap_presets_extension_t *,
                                                aap_presets_context_t *)> func) {
        auto presetsExtension = (aap_presets_extension_t *) instance->getExtension(
                AAP_PRESETS_EXTENSION_URI);
        aap_presets_context_t context;
        context.plugin = nullptr; // should not be necessary.
        context.context = this;
        func(presetsExtension, &context);
    }

public:
    PresetsPluginServiceExtension()
        : PluginServiceExtensionImpl(AAP_PRESETS_EXTENSION_URI) {
    }

    // invoked by AudioPluginService
    void onInvoked(void* contextInstance, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override;
};

/**
 * Used by internal extension developers (that is, AAP framework developers)
 */
class PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImpl> client;
    std::unique_ptr<PluginServiceExtensionImpl> service;
    AAPXSFeature pub;

    static void* internalAsProxy(AAPXSClientInstance* extension) {
        auto thisObj = (PluginExtensionFeatureImpl*) extension->context;
        return thisObj->client->asProxy(extension);
    }

    static void internalOnInvoked(void *service, AAPXSServiceInstance* extension, int32_t opcode) {
        auto thisObj = (PluginExtensionFeatureImpl*) extension->context;
        thisObj->service->onInvoked(service, extension, opcode);
    }

public:
    PluginExtensionFeatureImpl()
        : client(std::unique_ptr<PluginClientExtensionImpl>()),
        service(std::unique_ptr<PluginServiceExtensionImpl>()) {
        pub.as_proxy = internalAsProxy;
        pub.on_invoked = internalOnInvoked;
    }

    AAPXSFeature asPublicApi() { return pub; }
};

} // namespace aap


#endif //AAP_CORE_STANDARD_EXTENSIONS_IMPL_H
