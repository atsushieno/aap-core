

#ifndef AAP_CORE_MIDI_AAPXS_H
#define AAP_CORE_MIDI_AAPXS_H

#include <functional>
#include <future>
#include "aap/unstable/aapxs-vnext.h"
#include "aap/ext/midi.h"
#include "aap/core/aapxs/aapxs-runtime.h"

// plugin extension opcodes
const int32_t OPCODE_GET_MAPPING_POLICY = 0;

// host extension opcodes
// ... nothing?

const int32_t MIDI_SHARED_MEMORY_SIZE = AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t);

namespace aap::xs {
    class AAPXSDefinition_Midi : public AAPXSDefinitionWrapper {

        static void aapxs_midi_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_midi_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_midi_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_midi_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        AAPXSDefinition aapxs_midi{AAP_MIDI_EXTENSION_URI,
                                    this,
                                    MIDI_SHARED_MEMORY_SIZE,
                                    aapxs_midi_process_incoming_plugin_aapxs_request,
                                    aapxs_midi_process_incoming_host_aapxs_request,
                                    aapxs_midi_process_incoming_plugin_aapxs_reply,
                                    aapxs_midi_process_incoming_host_aapxs_reply
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_midi;
        }
    };

    class MidiClientAAPXS : public TypedAAPXS {
    public:
        MidiClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_MIDI_EXTENSION_URI, initiatorInstance, serialization) {
        }

        int32_t getMidiMappingPolicy();
    };

    class MidiServiceAAPXS : public TypedAAPXS {
    public:
        MidiServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_MIDI_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // nothing?
    };
}

#endif //AAP_CORE_MIDI_AAPXS_H
