
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

        AAPXSDefinition aapxs_state{AAP_STATE_EXTENSION_URI,
                                         this,
                                         STATE_SHARED_MEMORY_SIZE,
                                         aapxs_state_process_incoming_plugin_aapxs_request,
                                         aapxs_state_process_incoming_host_aapxs_request,
                                         aapxs_state_process_incoming_plugin_aapxs_reply,
                                         aapxs_state_process_incoming_host_aapxs_reply
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_state;
        }
    };

    class StateClientAAPXS : public TypedAAPXS {
    public:
        StateClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_STATE_EXTENSION_URI, initiatorInstance, serialization) {
        }

        int32_t getStateSize();
        void getState(aap_state_t& state);
        void setState(aap_state_t& state);
    };

    class StateServiceAAPXS : public TypedAAPXS {
    public:
        StateServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_STATE_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // nothing?
    };
}

#endif //AAP_CORE_STATE_AAPXS_H
