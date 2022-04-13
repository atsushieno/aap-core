
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

class AndroidAudioPluginServiceExtensionImpl {
protected:
    AndroidAudioPluginExtensionServiceClient *client;
    AndroidAudioPluginExtension *extensionInstance;

public:
    AndroidAudioPluginServiceExtensionImpl(AndroidAudioPluginExtensionServiceClient *extensionClient,
                                       AndroidAudioPluginExtension *extensionInstance)
            : client(extensionClient), extensionInstance(extensionInstance) {
    }

    /** Optionally override this for additional initialization and resource acquisition */
    virtual void initialize() {}

    /** Optionally override this for additional termination and resource releases */
    virtual void terminate() {}

    void clientInvokePluginExtension(int32_t opcode) {

    }

    virtual void onInvoked(AndroidAudioPluginExtensionServiceClient *client,
                           int32_t opcode) = 0;

    virtual void* asProxy(AndroidAudioPluginExtensionServiceClient *client) = 0;
};

class PresetsPluginServiceExtensionImpl : public AndroidAudioPluginServiceExtensionImpl {
    aap_presets_extension_t proxy{};

    class MemoryBlock {
        void* buffer{nullptr};
        int32_t size{0};

        ~MemoryBlock() {
            if (buffer)
                free(buffer);
        }

        void copyFrom(int32_t srcSize, void *src) {
            if (buffer == nullptr || size < srcSize) {
                if (buffer != nullptr)
                    free(buffer);
                buffer = calloc(1, srcSize);
            }
            memcpy(buffer, src, srcSize);
            size = srcSize;
        }
    };

    static int32_t internalGetPresetCount(aap_presets_context_t* context) {
        return ((PresetsPluginServiceExtensionImpl*)context->context)->getPresetCount();
    }

    static int32_t internalGetPresetDataSize(aap_presets_context_t* context, int32_t index) {
        return ((PresetsPluginServiceExtensionImpl*)context->context)->getPresetDataSize(index);
    }

    static void internalGetPreset(aap_presets_context_t* context, int32_t index, bool skipBinary, aap_preset_t *preset) {
        ((PresetsPluginServiceExtensionImpl*)context->context)->getPreset(index, skipBinary, preset);
    }

    static int32_t internalGetPresetIndex(aap_presets_context_t* context) {
        return ((PresetsPluginServiceExtensionImpl*)context->context)->getPresetIndex();
    }

    static void internalSetPresetIndex(aap_presets_context_t* context, int32_t index) {
        return ((PresetsPluginServiceExtensionImpl*)context->context)->setPresetIndex(index);
    }

public:
    PresetsPluginServiceExtensionImpl(AndroidAudioPluginExtensionServiceClient *extensionClient, AndroidAudioPluginExtension *extensionInstance)
            : AndroidAudioPluginServiceExtensionImpl(extensionClient, extensionInstance) {
    }

    void onInvoked(AndroidAudioPluginExtensionServiceClient *client,
                    int32_t opcode) override {

        int32_t index;
        //aap_presets_extension_t *presetsExtension;
        //aap_presets_context_t *presetsExtensionContext;
        //AndroidAudioPlugin *plugin;
        //aap_preset_t preset;
        switch (opcode) {
        case OPCODE_GET_PRESET_COUNT:
            assert(false); // FIXME: implement
        case OPCODE_GET_PRESET_DATA_SIZE:
            index = *((int32_t*) extensionInstance->data);
            /*
            // No, no, no, they need to be implemented without dealing with C facades. Just resort to aap::PluginClient.
            presetsExtensionContext = (aap_presets_context_t*) presetsExtension->context;
            plugin = presetsExtensionContext->plugin;
            presetsExtension = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
            *((int32_t*) extensionInstance->data) = presetsExtension->get_preset_size(presetsExtensionContext, index);
             */
            assert(false); // FIXME: implement
            break;
        case OPCODE_GET_PRESET_DATA:
            index = *((int32_t*) extensionInstance->data);
            /*
            // No, no, no, they need to be implemented without dealing with C facades. Just resort to aap::PluginClient.
            presetsExtensionContext = (aap_presets_context_t*) presetsExtension->context;
            plugin = presetsExtensionContext->plugin;
            presetsExtension = (aap_presets_extension_t*) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
            presetsExtension->get_preset(presetsExtensionContext, index, &preset);
            // write size, then data
            *((int32_t*) extensionInstance->data) = preset.data_size;
            memcpy((uint8_t*) extensionInstance->data + sizeof(int32_t), preset.data, preset.data_size);
            */
            assert(false); // FIXME: implement
            break;
        case OPCODE_GET_PRESET_INDEX:
            assert(false); // FIXME: implement
        case OPCODE_SET_PRESET_INDEX:
            index = *((int32_t*) extensionInstance->data);
            assert(false); // FIXME: implement
        default:
            assert(0); // should not reach here
            break;
        }
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
        proxy.get_preset_count = PresetsPluginServiceExtensionImpl::internalGetPresetCount;
        proxy.get_preset_data_size = PresetsPluginServiceExtensionImpl::internalGetPresetDataSize;
        proxy.get_preset = PresetsPluginServiceExtensionImpl::internalGetPreset;
        proxy.get_preset_index = PresetsPluginServiceExtensionImpl::internalGetPresetIndex;
        proxy.set_preset_index = PresetsPluginServiceExtensionImpl::internalSetPresetIndex;
        return &proxy;
    }
};

void aap_presets_plugin_service_extension_on_invoked(AndroidAudioPluginExtensionServiceClient *client, AndroidAudioPluginServiceExtension* ext, int32_t opcode) {
    auto impl = (AndroidAudioPluginServiceExtensionImpl*) ext->context;
    impl->onInvoked(client, opcode);
}

void* aap_presets_plugin_service_extension_as_proxy(AndroidAudioPluginExtensionServiceClient *client, AndroidAudioPluginServiceExtension* ext) {
    auto impl = (AndroidAudioPluginServiceExtensionImpl*) ext->context;
    return impl->asProxy(client);
}

aap_service_extension_t presets_plugin_service_extension{
    AAP_PRESETS_EXTENSION_URI,
    aap_presets_plugin_service_extension_on_invoked,
    aap_presets_plugin_service_extension_as_proxy};
