#include "port-config-service.h"

template<typename T>
void aap::PortConfigPluginServiceExtension::withPortConfigExtension(AndroidAudioPlugin* plugin, T defaultValue,
                                                                    std::function<void(aap_port_config_extension_t *,
                                                                                       AndroidAudioPluginExtensionTarget)> func) {
    // This instance->getExtension() should return an extension from the loaded plugin.
    assert(plugin);
    auto portConfigExtension = (aap_port_config_extension_t *) plugin->get_extension(plugin, AAP_PORT_CONFIG_EXTENSION_URI);
    assert(portConfigExtension);
    // We don't need context for service side.
    func(portConfigExtension, AndroidAudioPluginExtensionTarget{plugin, nullptr});
}

void aap::PortConfigPluginServiceExtension::onInvoked(AndroidAudioPlugin *plugin,
                                                      AAPXSServiceInstance *extensionInstance,
                                                      int32_t opcode) {
    switch (opcode) {
        case OPCODE_PORT_CONFIG_GET_OPTIONS:
            withPortConfigExtension<int32_t>(plugin, 0, [=](aap_port_config_extension_t *ext,
                                                        AndroidAudioPluginExtensionTarget target) {
                auto destination = (aap_port_config_t *)  extensionInstance->data;
                ext->get_options(target, destination);
                return 0;
            });
            break;
        case OPCODE_PORT_CONFIG_SELECT:
            withPortConfigExtension<int32_t>(plugin, 0, [=](aap_port_config_extension_t *ext,
                                                            AndroidAudioPluginExtensionTarget target) {
                int32_t index = *((int32_t *) extensionInstance->data);
                ext->select(target, index);
                return 0;
            });
            break;
    }
}

