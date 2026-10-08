#ifndef AAP_CORE_BUSES_AAPXS_H
#define AAP_CORE_BUSES_AAPXS_H

#include "aap/aapxs.h"
#include "../../ext/buses.h"
#include "typed-aapxs.h"

// The buses AAPXS has framework-level opcodes that are answered by the hosting framework
// (LocalPluginInstance), not by the plugin. The plugin's `aap_buses_extension_t` is consulted
// by the framework to build the layout.

// Hosts send these requests only to plugins that declare this extension in their metadata
// (`<extension uri="urn://androidaudioplugin.org/extensions/buses/v1" />`).

// plugin extension opcodes

// Request: none. Reply: aap_buses_layout_snapshot_t.
const int32_t OPCODE_BUSES_GET_LAYOUT = 1;

// The layout is reported by the plugin's buses extension. Otherwise the client derives buses
// from the legacy port configuration, exactly like the service does.
#define AAP_BUSES_LAYOUT_PLUGIN_PROVIDED 1

typedef struct aap_buses_layout_snapshot_t {
    uint32_t generation;
    uint32_t flags;
    int32_t count;
    aap_bus_info_t buses[AAP_MAX_BUSES];
} aap_buses_layout_snapshot_t;

const int32_t BUSES_SHARED_MEMORY_SIZE = sizeof(aap_buses_layout_snapshot_t);

namespace aap::xs {
    // Implemented by the service-side hosting framework, which answers the framework-level
    // opcodes (see LocalPluginInstance::getBusesServiceHandler()).
    class BusesServiceHandler {
    public:
        virtual ~BusesServiceHandler() = default;
        virtual void getBusLayoutSnapshot(aap_buses_layout_snapshot_t& snapshot) = 0;
    };

    class BusesClientAAPXS : public TypedAAPXS {
    public:
        BusesClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_BUSES_EXTENSION_URI, initiatorInstance, serialization) {
        }

        Result<aap_buses_layout_snapshot_t> getLayout();
    };

    class AAPXSDefinition_Buses : public AAPXSDefinitionWrapper {

        static void aapxs_buses_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_buses_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_buses_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_buses_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        AAPXSDefinition aapxs_buses{
            this,
            AAP_BUSES_EXTENSION_URI,
            BUSES_SHARED_MEMORY_SIZE,
            aapxs_buses_process_incoming_plugin_aapxs_request,
            aapxs_buses_process_incoming_host_aapxs_request,
            aapxs_buses_process_incoming_plugin_aapxs_reply,
            aapxs_buses_process_incoming_host_aapxs_reply,
            nullptr, // no C plugin extension proxy; the framework uses BusesClientAAPXS directly.
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            initializeTypedAAPXSInitiator<BusesClientAAPXS>,
            nullptr,
            releaseTypedAAPXSInitiator,
            nullptr
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_buses;
        }
    };
}

#endif //AAP_CORE_BUSES_AAPXS_H
