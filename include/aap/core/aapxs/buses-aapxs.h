#ifndef AAP_CORE_BUSES_AAPXS_H
#define AAP_CORE_BUSES_AAPXS_H

#include "aap/aapxs.h"
#include "../../ext/buses.h"
#include "typed-aapxs.h"
#include <algorithm>

// The buses AAPXS has framework-level opcodes that are answered by the hosting framework
// (LocalPluginInstance), not by the plugin. The plugin's `aap_buses_extension_t` is consulted
// by the framework to build the layout.

// Hosts send these requests only to plugins that declare this extension in their metadata
// (`<extension uri="urn://androidaudioplugin.org/extensions/buses/v1" />`).

// plugin extension opcodes

// Request: none. Reply: aap_buses_layout_snapshot_t.
const int32_t OPCODE_BUSES_GET_LAYOUT = 1;
// Request: aap_buffer_layout_t. Reply: int32_t, 1 if accepted.
// Sent before beginPrepare(); then prepareMemory() passes the single pool FD (index 0).
const int32_t OPCODE_BUSES_COMMIT_BUFFER_LAYOUT = 2;
// Request: aap_bus_layout_request_t. Reply: int32_t, 1 if the plugin accepted it.
// The bus layout generation changes; the host re-reads the layout and prepares again.
const int32_t OPCODE_BUSES_APPLY_LAYOUT = 3;

// host extension opcodes: payload-free notifications, coalesced (see notify_buses_changed()).
const int32_t OPCODE_NOTIFY_BUS_NAMES_CHANGED = -1;
const int32_t OPCODE_NOTIFY_BUS_LAYOUT_CHANGED = -2;

// The layout is reported by the plugin's buses extension. Otherwise the client derives buses
// from the legacy port configuration, exactly like the service does.
#define AAP_BUSES_LAYOUT_PLUGIN_PROVIDED 1

typedef struct aap_buses_layout_snapshot_t {
    uint32_t generation;
    uint32_t flags;
    int32_t count;
    aap_bus_info_t buses[AAP_MAX_BUSES];
} aap_buses_layout_snapshot_t;

#define AAP_MAX_BUFFER_LAYOUT_ENTRIES 256

// Where each port buffer lives in the shared memory pool of an instance.
// Entries are indexed by port index (the flattened bus channels, then the event buses).
typedef struct aap_buffer_layout_entry_t {
    uint32_t offset;
    uint32_t size;
} aap_buffer_layout_entry_t;

typedef struct aap_buffer_layout_t {
    // The bus layout generation that this buffer layout is based on.
    uint32_t generation;
    uint32_t pool_size;
    uint32_t frame_capacity;
    int32_t entry_count;
    aap_buffer_layout_entry_t entries[AAP_MAX_BUFFER_LAYOUT_ENTRIES];
} aap_buffer_layout_t;

constexpr int32_t BUSES_SHARED_MEMORY_SIZE = std::max({sizeof(aap_buses_layout_snapshot_t),
                                                       sizeof(aap_buffer_layout_t),
                                                       sizeof(aap_bus_layout_request_t)});

namespace aap::xs {
    // Implemented by the service-side hosting framework, which answers the framework-level
    // opcodes (see LocalPluginInstance::getBusesServiceHandler()).
    class BusesServiceHandler {
    public:
        virtual ~BusesServiceHandler() = default;
        virtual void getBusLayoutSnapshot(aap_buses_layout_snapshot_t& snapshot) = 0;
        // Returns false if it is rejected (e.g. a stale generation, or the instance is active).
        virtual bool commitBufferLayout(const aap_buffer_layout_t& layout) = 0;
        // Returns false if the plugin rejects it (or cannot change its layout at all).
        virtual bool applyLayout(const aap_bus_layout_request_t& request) = 0;
    };

    class BusesClientAAPXS : public TypedAAPXS {
    public:
        BusesClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_BUSES_EXTENSION_URI, initiatorInstance, serialization) {
        }

        Result<aap_buses_layout_snapshot_t> getLayout();
        Result<bool> commitBufferLayout(const aap_buffer_layout_t& layout);
        Result<bool> applyLayout(const aap_bus_layout_request_t& request);
    };

    class BusesServiceAAPXS : public TypedAAPXS {
        static void staticNotifyBusesChanged(aap_buses_host_extension_t* ext, AndroidAudioPluginHost*, uint32_t flags) {
            ((BusesServiceAAPXS*) ext->aapxs_context)->notifyBusesChanged(flags);
        }
        aap_buses_host_extension_t host_extension{this, staticNotifyBusesChanged};

    public:
        BusesServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_BUSES_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyBusesChanged(uint32_t flags);

        aap_buses_host_extension_t* asHostExtension() { return &host_extension; }
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

        static AAPXSExtensionServiceProxy aapxs_buses_get_host_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);
        static void* aapxs_buses_as_host_extension(AAPXSExtensionServiceProxy* proxy) {
            return ((BusesServiceAAPXS*) proxy->aapxs_context)->asHostExtension();
        }
        static uint32_t aapxs_buses_request_flags(AAPXSDefinition*, bool host, int32_t opcode) {
            return host && (opcode == OPCODE_NOTIFY_BUS_NAMES_CHANGED || opcode == OPCODE_NOTIFY_BUS_LAYOUT_CHANGED) ?
                   AAPXS_REQUEST_COALESCE : 0u;
        }

        AAPXSDefinition aapxs_buses{
            this,
            AAP_BUSES_EXTENSION_URI,
            BUSES_SHARED_MEMORY_SIZE,
            aapxs_buses_process_incoming_plugin_aapxs_request,
            aapxs_buses_process_incoming_host_aapxs_request,
            aapxs_buses_process_incoming_plugin_aapxs_reply,
            aapxs_buses_process_incoming_host_aapxs_reply,
            nullptr, // no C plugin extension proxy; the framework uses BusesClientAAPXS directly.
            aapxs_buses_get_host_proxy,
            nullptr,
            nullptr,
            nullptr,
            aapxs_buses_request_flags,
            nullptr,
            nullptr,
            nullptr,
            initializeTypedAAPXSInitiator<BusesClientAAPXS, BusesServiceAAPXS>,
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
