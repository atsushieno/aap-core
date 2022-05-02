
#include "presets-service.h"

namespace aap {

template<typename T>
void PresetsPluginServiceExtension::withPresetExtension(aap::LocalPluginInstance *instance,
                                                        T defaultValue,
                                                        std::function<void(aap_presets_extension_t *, aap_presets_context_t *)> func) {
    // This instance->getExtension() should return an extension from the loaded plugin.
    auto plugin = instance->getPlugin();
    assert(plugin);
    auto presetsExtension = (aap_presets_extension_t *) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
    assert(presetsExtension);
    aap_presets_context_t context;
    context.plugin = plugin;
    context.context = presetsExtension->context;
    func(presetsExtension, &context);
}

void PresetsPluginServiceExtension::onInvoked(void* contextInstance, AAPXSServiceInstance *extensionInstance,
                                              int32_t opcode) {
    auto instance = (aap::LocalPluginInstance *) contextInstance;

    int32_t index;
    switch (opcode) {
    case OPCODE_GET_PRESET_COUNT:
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            *((int32_t *) extensionInstance->data) = ext ? ext->get_preset_count(context)
                                                         : 0;
        });
        break;
    case OPCODE_GET_PRESET_DATA_SIZE:
        index = *((int32_t *) extensionInstance->data);
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            *((int32_t *) extensionInstance->data) = ext ? ext->get_preset_data_size(
                    context, index) : 0;
        });
        break;
    case OPCODE_GET_PRESET_DATA:
        index = *((int32_t *) extensionInstance->data);
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            if (ext != nullptr) {
                aap_preset_t preset;
                preset.data = (uint8_t *) extensionInstance->data + sizeof(int32_t) +
                              sizeof(preset.name);
                ext->get_preset(context, index, true, &preset);
                strncpy((char *) (uint8_t *) extensionInstance->data + sizeof(int32_t),
                        const_cast<char *const>(preset.name), sizeof(preset.name));
            } else {
                *((int32_t *) extensionInstance->data) = 0; // empty data
            }
        });
        break;
    case OPCODE_GET_PRESET_INDEX:
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            *((int32_t *) extensionInstance->data) = ext ? ext->get_preset_index(context)
                                                         : 0;
        });
        break;
    case OPCODE_SET_PRESET_INDEX:
        index = *((int32_t *) extensionInstance->data);
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            if (ext != nullptr)
                ext->set_preset_index(context, index);
        });
        break;
    default:
        assert(0); // should not reach here
        break;
    }
}

} // namespace aap
