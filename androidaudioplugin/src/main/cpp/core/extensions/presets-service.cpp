
#include "aap/android-audio-plugin.h"
#include "aap/unstable/extensions.h"
#include "aap/unstable/presets.h"
#include "aap/core/host/audio-plugin-host.h"
#include <functional>

// implementation details
const int32_t OPCODE_GET_PRESET_COUNT = 0;
const int32_t OPCODE_GET_PRESET_DATA_SIZE = 1;
const int32_t OPCODE_GET_PRESET_DATA = 2;
const int32_t OPCODE_GET_PRESET_INDEX = 3;
const int32_t OPCODE_SET_PRESET_INDEX = 4;

class AndroidAudioPluginClientExtensionImpl {
protected:
    AndroidAudioPluginExtensionServiceClient *client;
    AndroidAudioPluginExtension *extensionInstance;

public:
    AndroidAudioPluginClientExtensionImpl(AndroidAudioPluginExtensionServiceClient *extensionClient,
                                       AndroidAudioPluginExtension *extensionInstance)
            : client(extensionClient), extensionInstance(extensionInstance) {
    }

    /** Optionally override this for additional initialization and resource acquisition */
    virtual void initialize() {}

    /** Optionally override this for additional termination and resource releases */
    virtual void terminate() {}

    void clientInvokePluginExtension(int32_t opcode) {
        // FIXME: implement
    }

    virtual void* asProxy(AndroidAudioPluginExtensionServiceClient *client) = 0;
};

class PresetsPluginClientExtensionImpl : public AndroidAudioPluginClientExtensionImpl {
    aap_presets_extension_t proxy{};

    static int32_t internalGetPresetCount(aap_presets_context_t* context) {
        return ((PresetsPluginClientExtensionImpl*)context->context)->getPresetCount();
    }

    static int32_t internalGetPresetDataSize(aap_presets_context_t* context, int32_t index) {
        return ((PresetsPluginClientExtensionImpl*)context->context)->getPresetDataSize(index);
    }

    static void internalGetPreset(aap_presets_context_t* context, int32_t index, bool skipBinary, aap_preset_t *preset) {
        ((PresetsPluginClientExtensionImpl*)context->context)->getPreset(index, skipBinary, preset);
    }

    static int32_t internalGetPresetIndex(aap_presets_context_t* context) {
        return ((PresetsPluginClientExtensionImpl*)context->context)->getPresetIndex();
    }

    static void internalSetPresetIndex(aap_presets_context_t* context, int32_t index) {
        return ((PresetsPluginClientExtensionImpl*)context->context)->setPresetIndex(index);
    }

public:
    PresetsPluginClientExtensionImpl(AndroidAudioPluginExtensionServiceClient *extensionClient, AndroidAudioPluginExtension *extensionInstance)
    : AndroidAudioPluginClientExtensionImpl(extensionClient, extensionInstance) {
    }

    int32_t getPresetCount() {
        clientInvokePluginExtension(OPCODE_GET_PRESET_COUNT);
        return *((int32_t*) extensionInstance->data);
    }

    int32_t getPresetDataSize(int32_t index) {
        *((int32_t*) extensionInstance->data) = index;
        clientInvokePluginExtension(OPCODE_GET_PRESET_DATA_SIZE);
        return *((int32_t*) extensionInstance->data);
    }

    void getPreset(int32_t index, bool skipBinary, aap_preset_t *result) {
        *((int32_t*) extensionInstance->data) = index;
        *(bool*) ((int32_t*) extensionInstance->data + 1) = skipBinary;
        clientInvokePluginExtension(OPCODE_GET_PRESET_DATA);
        result->data_size = *((int32_t*) extensionInstance->data);
        strncpy(result->name, (const char*) ((int32_t*) extensionInstance->data + 1), 256);
        memcpy(result->data, ((int32_t*) extensionInstance->data + 1), result->data_size);
    }

    int32_t getPresetIndex() {
        clientInvokePluginExtension(OPCODE_GET_PRESET_INDEX);
        return *((int32_t*) extensionInstance->data);
    }

    void setPresetIndex(int32_t index) {
        *((int32_t*) extensionInstance->data) = index;
        clientInvokePluginExtension(OPCODE_SET_PRESET_INDEX);
    }

    void* asProxy(AndroidAudioPluginExtensionServiceClient *client) override {
        proxy.context = this;
        proxy.get_preset_count = internalGetPresetCount;
        proxy.get_preset_data_size = internalGetPresetDataSize;
        proxy.get_preset = internalGetPreset;
        proxy.get_preset_index = internalGetPresetIndex;
        proxy.set_preset_index = internalSetPresetIndex;
        return &proxy;
    }
};

class AndroidAudioPluginServiceExtensionImpl {
protected:
    AndroidAudioPluginExtension *extensionInstance{nullptr};

public:
    explicit AndroidAudioPluginServiceExtensionImpl(AndroidAudioPluginExtension *extensionInstance)
            : extensionInstance(extensionInstance) {
    }

    /** Optionally override this for additional initialization and resource acquisition */
    virtual void initialize() {}

    /** Optionally override this for additional termination and resource releases */
    virtual void terminate() {}

    virtual void onInvoked(aap::LocalPluginInstance *instance,
                           int32_t opcode) = 0;
};

class PresetsPluginServiceExtensionImpl : public AndroidAudioPluginServiceExtensionImpl {

    template <typename T>
    void withPresetExtension(aap::LocalPluginInstance *instance, T defaultValue, std::function<void(aap_presets_extension_t*, aap_presets_context_t*)> func) {
        auto presetsExtension = (aap_presets_extension_t*) instance->getExtension(AAP_PRESETS_EXTENSION_URI);
        aap_presets_context_t context;
        context.plugin = nullptr; // should not be necessary.
        context.context = this;
        func(presetsExtension, &context);
    }

public:
    PresetsPluginServiceExtensionImpl(AndroidAudioPluginExtension *extensionInstance)
            : AndroidAudioPluginServiceExtensionImpl(extensionInstance) {
    }

    // invoked by AudioPluginService
    void onInvoked(aap::LocalPluginInstance *instance,
                    int32_t opcode) override {

        int32_t index;
        switch (opcode) {
        case OPCODE_GET_PRESET_COUNT:
            withPresetExtension<int32_t>(instance, 0, [&](aap_presets_extension_t *ext, aap_presets_context_t *context) {
                *((int32_t*) extensionInstance->data) = ext ? ext->get_preset_count(context) : 0;
            });
            break;
        case OPCODE_GET_PRESET_DATA_SIZE:
            index = *((int32_t*) extensionInstance->data);
            withPresetExtension<int32_t>(instance, 0, [&](aap_presets_extension_t *ext, aap_presets_context_t *context) {
                *((int32_t*) extensionInstance->data) = ext ? ext->get_preset_data_size(context, index) : 0;
            });
            break;
        case OPCODE_GET_PRESET_DATA:
            index = *((int32_t*) extensionInstance->data);
            withPresetExtension<int32_t>(instance, 0, [&](aap_presets_extension_t *ext, aap_presets_context_t *context) {
                if (ext != nullptr) {
                    aap_preset_t preset;
                    preset.data = (uint8_t*) extensionInstance->data + sizeof(int32_t) + sizeof(preset.name);
                    ext->get_preset(context, index, true, &preset);
                    strncpy((char*) (uint8_t*) extensionInstance->data + sizeof(int32_t), const_cast<char* const>(preset.name), sizeof(preset.name));
                } else {
                    *((int32_t*) extensionInstance->data) = 0; // empty data
                }
            });
            break;
        case OPCODE_GET_PRESET_INDEX:
            withPresetExtension<int32_t>(instance, 0, [&](aap_presets_extension_t *ext, aap_presets_context_t *context) {
                *((int32_t*) extensionInstance->data) = ext ? ext->get_preset_index(context) : 0;
            });
            break;
        case OPCODE_SET_PRESET_INDEX:
            index = *((int32_t*) extensionInstance->data);
            withPresetExtension<int32_t>(instance, 0, [&](aap_presets_extension_t *ext, aap_presets_context_t *context) {
                if (ext != nullptr)
                    ext->set_preset_index(context, index);
            });
            break;
        default:
            assert(0); // should not reach here
            break;
        }
    }
};

void aap_presets_plugin_service_extension_on_invoked(void *service, AndroidAudioPluginServiceExtension* ext, int32_t opcode) {
    auto instance = (aap::LocalPluginInstance*) service;
    auto impl = (PresetsPluginServiceExtensionImpl*) ext->context;
    impl->onInvoked(instance, opcode);
}

void* aap_presets_plugin_service_extension_as_proxy(AndroidAudioPluginExtensionServiceClient *client, AndroidAudioPluginServiceExtension* ext) {
    auto impl = (PresetsPluginClientExtensionImpl*) ext->context;
    return impl->asProxy(client);
}

AndroidAudioPluginExtensionFeature presets_plugin_service_extension{
    AAP_PRESETS_EXTENSION_URI,
    aap_presets_plugin_service_extension_on_invoked,
    aap_presets_plugin_service_extension_as_proxy};
