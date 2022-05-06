
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

    switch (opcode) {
    case OPCODE_GET_PRESET_COUNT:
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            *((int32_t *) extensionInstance->data) = ext ? ext->get_preset_count(context)
                                                         : 0;
        });
        break;
    case OPCODE_GET_PRESET_DATA_SIZE:
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            int32_t index = *((int32_t *) extensionInstance->data);
            *((int32_t *) extensionInstance->data) = ext ? ext->get_preset_data_size(
                    context, index) : 0;
        });
        break;
    case OPCODE_GET_PRESET_DATA:
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            // request (offset-range: content)
            // - 0..3 : int32_t index
            // - 4..7 : bool skip binary or not
            aap_preset_t preset;
            int32_t index = *((int32_t *) extensionInstance->data);
            bool skipBinary = *((int32_t *) extensionInstance->data + 1) != 0;
            // set its destination location to the shared buffer buffer, offset to the actual data (int size + nameLength).
            if (!skipBinary)
                preset.data = (uint8_t *) extensionInstance->data + sizeof(int32_t) + AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH;
            ext->get_preset(context, index, skipBinary, &preset);
            // response (offset-range: content)
            // - 0..3 : data size
            // - 4..259 : name (fixed length char buffer)
            // - 260..* : data (if requested)
            *((int32_t*) extensionInstance->data) = preset.data_size;
            strncpy((char *) extensionInstance->data + sizeof(int32_t),
                    const_cast<char *const>(preset.name), AAP_PRESETS_EXTENSION_MAX_NAME_LENGTH);
            // preset.data is already assigned to the shm buffer, so no need to memcpy.
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
        withPresetExtension<int32_t>(instance, 0, [=](aap_presets_extension_t *ext,
                                                      aap_presets_context_t *context) {
            int32_t index = *((int32_t *) extensionInstance->data);
            ext->set_preset_index(context, index);
        });
        break;
    default:
        assert(0); // should not reach here
        break;
    }
}

} // namespace aap
