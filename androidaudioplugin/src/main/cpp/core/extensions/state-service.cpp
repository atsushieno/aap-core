
#include "state-service.h"

std::map<AndroidAudioPlugin*,aap::StatePluginClientExtension::Instance*> state_xs_instance_map{};

namespace aap {

    template<typename T>
    void StatePluginServiceExtension::withStateExtension(aap::LocalPluginInstance *instance,
                                                            T defaultValue,
                                                            std::function<void(aap_state_extension_t *, AndroidAudioPlugin *)> func) {
        // This instance->getExtension() should return an extension from the loaded plugin.
        auto plugin = instance->getPlugin();
        assert(plugin);
        auto stateExtension = (aap_state_extension_t *) plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI);
        assert(stateExtension);
        func(stateExtension, plugin);
    }

    void StatePluginServiceExtension::onInvoked(void* contextInstance, AAPXSServiceInstance *extensionInstance,
                                                  int32_t opcode) {
        auto instance = (aap::LocalPluginInstance *) contextInstance;

        switch (opcode) {
            case OPCODE_GET_STATE_SIZE:
                withStateExtension<int32_t>(instance, 0, [=](aap_state_extension_t *ext,
                                                              AndroidAudioPlugin *plugin) {
                    *((int32_t *) extensionInstance->data) = ext->get_state_size(plugin);
                });
                break;
            case OPCODE_GET_STATE:
                withStateExtension<int32_t>(instance, 0, [=](aap_state_extension_t *ext,
                                                             AndroidAudioPlugin *plugin) {
                    aap_state_t state;
                    state.data = extensionInstance->data;
                    ext->get_state(plugin, &state);
                });
                break;
            case OPCODE_SET_STATE:
                withStateExtension<int32_t>(instance, 0, [=](aap_state_extension_t *ext,
                                                             AndroidAudioPlugin *plugin) {
                    aap_state_t state;
                    state.data = extensionInstance->data;
                    ext->set_state(plugin, &state);
                });
            default:
                assert(0); // should not reach here
                break;
        }
    }

    StatePluginClientExtension::Instance::Instance(StatePluginClientExtension *owner,
                                                   AAPXSClientInstance *clientInstance)
            : owner(owner)
    {
        aapxsInstance = clientInstance;
    }

    int32_t
    StatePluginClientExtension::Instance::internalGetStateSize(AndroidAudioPlugin *plugin) {
        return state_xs_instance_map[plugin] ? state_xs_instance_map[plugin]->getStateSize() : 0;
    }

    void StatePluginClientExtension::Instance::internalGetState(AndroidAudioPlugin *plugin,
                                                                aap_state_t *state) {
        if (state_xs_instance_map[plugin])
            state_xs_instance_map[plugin]->getState(state);
    }

    void StatePluginClientExtension::Instance::internalSetState(AndroidAudioPlugin *plugin,
                                                                aap_state_t *state) {
        if (state_xs_instance_map[plugin])
            state_xs_instance_map[plugin]->setState(state);
    }

} // namespace aap
