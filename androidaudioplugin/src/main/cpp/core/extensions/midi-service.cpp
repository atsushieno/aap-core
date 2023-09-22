#include <aap/android-audio-plugin.h>
#include "midi-service.h"

int32_t getMidiSettingsFromLocalConfig(std::string pluginId) {
    return aap::AAPJniFacade::getInstance()->getMidiSettingsFromLocalConfig(pluginId);
}

void aap::MidiPluginServiceExtension::onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance, int32_t opcode) {
    switch (opcode) {
        case OPCODE_GET_MAPPING_POLICY:
            auto len = *(int32_t *) extensionInstance->data;
            assert(len < AAP_MAX_PLUGIN_ID_SIZE);
            char *pluginId = (char *) calloc(len + 1, 1);
            strncpy(pluginId, (const char *) ((int32_t *) extensionInstance->data + 1), len);
            *((int32_t *) extensionInstance->data) = getMidiSettingsFromLocalConfig(pluginId);
            return;
    }
    assert(false); // should not happen
}
