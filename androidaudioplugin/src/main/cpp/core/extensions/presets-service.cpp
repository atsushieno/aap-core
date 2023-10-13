
#include "presets-service.h"

namespace aap {

template<typename T>
void PresetsPluginServiceExtension::withPresetExtension(AndroidAudioPlugin* plugin,
                                                        T defaultValue,
                                                        std::function<void(aap_presets_extension_t *, AndroidAudioPlugin*)> func) {
    // This instance->getExtension() should return an extension from the loaded plugin.
    assert(plugin);
    auto presetsExtension = (aap_presets_extension_t *) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
    assert(presetsExtension);
    func(presetsExtension, plugin);
}

void PresetsPluginServiceExtension::onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                                              int32_t opcode) {
    switch (opcode) {
    case OPCODE_GET_PRESET_COUNT:
        withPresetExtension<int32_t>(plugin, 0, [=](aap_presets_extension_t *ext,
                                                      AndroidAudioPlugin* plugin) {
            *((int32_t *) extensionInstance->data) = ext ? ext->get_preset_count(ext, plugin)
                                                         : 0;
        });
        break;
    case OPCODE_GET_PRESET_DATA:
        withPresetExtension<int32_t>(plugin, 0, [=](aap_presets_extension_t *ext,
                                                    AndroidAudioPlugin* plugin) {
            // request (offset-range: content)
            // - 0..3 : int32_t index
            // - 4..7 : bool skip binary or not
            aap_preset_t preset;
            int32_t index = *((int32_t *) extensionInstance->data);
            ext->get_preset(ext, plugin, index, &preset, nullptr, nullptr); // we can pass null callback as plugins implement it in synchronous way
            // response (offset-range: content)
            // - 0..3 : stable ID
            // - 4..259 : name (fixed length char buffer)
            *((int32_t*) extensionInstance->data) = preset.id;
            size_t len = strlen(preset.name);
            strncpy((char *) extensionInstance->data + sizeof(int32_t),
                    const_cast<char *const>(preset.name), len);
            ((char *) extensionInstance->data)[len + sizeof(int32_t)] = 0;
        });
        break;
    case OPCODE_GET_PRESET_INDEX:
        withPresetExtension<int32_t>(plugin, 0, [=](aap_presets_extension_t *ext,
                                                    AndroidAudioPlugin* plugin) {
            *((int32_t *) extensionInstance->data) = ext ? ext->get_preset_index(ext, plugin) : 0;
        });
        break;
    case OPCODE_SET_PRESET_INDEX:
        withPresetExtension<int32_t>(plugin, 0, [=](aap_presets_extension_t *ext,
                                                    AndroidAudioPlugin* plugin) {
            int32_t index = *((int32_t *) extensionInstance->data);
            ext->set_preset_index(ext, plugin, index);
        });
        break;
    default:
        assert(0); // should not reach here
        break;
    }
}

} // namespace aap
