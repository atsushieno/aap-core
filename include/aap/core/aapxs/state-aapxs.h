
#ifndef AAP_CORE_STATE_AAPXS_H
#define AAP_CORE_STATE_AAPXS_H

#include <functional>
#include <future>
#include "aap/aapxs.h"
#include "../../ext/state.h"
#include "typed-aapxs.h"

// plugin extension opcodes
const int32_t OPCODE_GET_STATE_SIZE = 0;
const int32_t OPCODE_GET_STATE = 1;
const int32_t OPCODE_SET_STATE = 2;

// host extension opcodes
// ... nothing?

const int32_t STATE_SHARED_MEMORY_SIZE = 0x100000; // 1M

namespace aap::xs {
    class StateClientAAPXS : public TypedAAPXS {
        // extension proxy support
        static size_t staticGetStateSize(aap_state_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ((StateClientAAPXS*) ext->aapxs_context)->getStateSize();
        }
        static void staticGetState(aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* stateToSave) {
            return ((StateClientAAPXS*) ext->aapxs_context)->getState(*stateToSave);
        }
        static void staticSetState(aap_state_extension_t* ext, AndroidAudioPlugin* plugin, aap_state_t* stateToLoad) {
            return ((StateClientAAPXS*) ext->aapxs_context)->setState(*stateToLoad);
        }
        aap_state_extension_t as_plugin_extension{this,
                                                  staticGetStateSize,
                                                  staticGetState,
                                                  staticSetState};

    public:
        StateClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_STATE_EXTENSION_URI, initiatorInstance, serialization) {
        }

        size_t getStateSize();
        void getState(aap_state_t& stateToSave);
        void setState(aap_state_t& stateToLoad);

        aap_state_extension_t * asPluginExtension() { return &as_plugin_extension; }
    };

    class StateServiceAAPXS : public TypedAAPXS {
    public:
        StateServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_STATE_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // nothing?
    };

    class AAPXSDefinition_State : public AAPXSDefinitionWrapper {

        static void aapxs_state_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_state_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_state_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_state_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        // It is used in synchronous context such as `get_extension()` in `binder-client-as-plugin.cpp` etc.
        static AAPXSExtensionClientProxy aapxs_state_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);

        static void* aapxs_parameters_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            return ((StateClientAAPXS*) proxy->aapxs_context)->asPluginExtension();
        }

        AAPXSDefinition aapxs_state{     this,
                                         AAP_STATE_EXTENSION_URI,
                                         STATE_SHARED_MEMORY_SIZE,
                                         aapxs_state_process_incoming_plugin_aapxs_request,
                                         aapxs_state_process_incoming_host_aapxs_request,
                                         aapxs_state_process_incoming_plugin_aapxs_reply,
                                         aapxs_state_process_incoming_host_aapxs_reply,
                                         aapxs_state_get_plugin_proxy,
                                         nullptr // no host extension
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_state;
        }
    };
}

#endif //AAP_CORE_STATE_AAPXS_H
