

#ifndef AAP_CORE_MIDI_AAPXS_H
#define AAP_CORE_MIDI_AAPXS_H

#include <functional>
#include <future>
#include "aap/aapxs.h"
#include "../../ext/midi.h"
#include "typed-aapxs.h"

// plugin extension opcodes
const int32_t OPCODE_GET_MAPPING_POLICY = 1;

// host extension opcodes
// ... nothing?

const int32_t MIDI_SHARED_MEMORY_SIZE = AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t);

namespace aap::xs {
    class MidiClientAAPXS : public TypedAAPXS {
        static enum aap_midi_mapping_policy staticGetMidiMappingPolicy(aap_midi_extension_t* ext, AndroidAudioPlugin*) {
            return ((MidiClientAAPXS*) ext->aapxs_context)->getMidiMappingPolicy();
        }
        aap_midi_extension_t as_public_extension{this, staticGetMidiMappingPolicy};
    public:
        MidiClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_MIDI_EXTENSION_URI, initiatorInstance, serialization) {
        }

        enum aap_midi_mapping_policy getMidiMappingPolicy();

        aap_midi_extension_t* asPluginExtension() { return &as_public_extension; }
    };

    class MidiServiceAAPXS : public TypedAAPXS {
    public:
        MidiServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_MIDI_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // nothing?
    };

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

        // It is used in synchronous context such as `get_extension()` in `binder-client-as-plugin.cpp` etc.
        static AAPXSExtensionClientProxy aapxs_midi_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);

        static void* aapxs_midi_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            return ((MidiClientAAPXS*) proxy->aapxs_context)->asPluginExtension();
        }

        AAPXSDefinition aapxs_midi{this,
                                   AAP_MIDI_EXTENSION_URI,
                                   MIDI_SHARED_MEMORY_SIZE,
                                   aapxs_midi_process_incoming_plugin_aapxs_request,
                                   aapxs_midi_process_incoming_host_aapxs_request,
                                   aapxs_midi_process_incoming_plugin_aapxs_reply,
                                   aapxs_midi_process_incoming_host_aapxs_reply,
                                   aapxs_midi_get_plugin_proxy,
                                   nullptr // no host extension
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_midi;
        }
    };
}

#endif //AAP_CORE_MIDI_AAPXS_H
