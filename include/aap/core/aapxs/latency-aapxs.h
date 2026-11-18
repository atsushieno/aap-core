#ifndef AAP_CORE_LATENCY_AAPXS_H
#define AAP_CORE_LATENCY_AAPXS_H

#include <functional>
#include "aap/aapxs.h"
#include "../../ext/latency.h"
#include "typed-aapxs.h"

// plugin extension opcodes

// Request: none. Reply: int32_t latency in samples.
const int32_t OPCODE_GET_LATENCY = 1;

// host extension opcodes
const int32_t OPCODE_NOTIFY_LATENCY_CHANGED = -1;

const int32_t LATENCY_SHARED_MEMORY_SIZE = sizeof(int32_t);

namespace aap::xs {

    class LatencyClientAAPXS : public TypedAAPXS {
        static int32_t staticGetLatency(aap_latency_extension_t* ext, AndroidAudioPlugin*) {
            auto result = ((LatencyClientAAPXS*) ext->aapxs_context)->getLatency();
            return result.isOk() ? result.value : 0;
        }
        aap_latency_extension_t as_plugin_extension{this, staticGetLatency};

    public:
        LatencyClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_LATENCY_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // Blocking-sync. Not for processing threads; cache the value and refresh it on notification.
        Result<int32_t> getLatency();
        // Asynchronous. Returns the request id; the callback fires exactly once on reply/timeout/death.
        int32_t getLatencyAsync(std::function<void(Result<int32_t>)> callback);

        aap_latency_extension_t* asPluginExtension() { return &as_plugin_extension; }
    };

    class LatencyServiceAAPXS : public TypedAAPXS {
        static void staticNotifyLatencyChanged(aap_latency_host_extension_t* ext, AndroidAudioPluginHost*) {
            ((LatencyServiceAAPXS*) ext->aapxs_context)->notifyLatencyChanged();
        }
        aap_latency_host_extension_t as_host_extension{this, staticNotifyLatencyChanged};

    public:
        LatencyServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_LATENCY_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyLatencyChanged();

        aap_latency_host_extension_t* asHostExtension() { return &as_host_extension; }
    };

    class AAPXSDefinition_Latency : public AAPXSDefinitionWrapper {

        static void aapxs_latency_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_latency_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_latency_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_latency_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        static AAPXSExtensionClientProxy aapxs_latency_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);
        static AAPXSExtensionServiceProxy aapxs_latency_get_host_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);
        static AAPXSExtensionHostReceiver aapxs_latency_get_host_receiver(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host);

        static uint32_t aapxs_latency_request_flags(AAPXSDefinition*, bool, int32_t);

        static void* aapxs_latency_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            auto* client = (LatencyClientAAPXS*) proxy->aapxs_context;
            return client ? client->asPluginExtension() : nullptr;
        }
        static void* aapxs_latency_as_host_extension(AAPXSExtensionServiceProxy* proxy) {
            auto* service = (LatencyServiceAAPXS*) proxy->aapxs_context;
            return service ? service->asHostExtension() : nullptr;
        }
        static void* aapxs_latency_as_host_receiver(AAPXSExtensionHostReceiver* receiver);

        // latency_in_samples is RT_UNSAFE (ext/latency.h), so the query always takes the Binder
        // route and runs under control exclusion; there is no snapshot and no is_command_rt_safe.
        AAPXSDefinition aapxs_latency{
            this,
            AAP_LATENCY_EXTENSION_URI,
            LATENCY_SHARED_MEMORY_SIZE,
            aapxs_latency_process_incoming_plugin_aapxs_request,
            aapxs_latency_process_incoming_host_aapxs_request,
            aapxs_latency_process_incoming_plugin_aapxs_reply,
            aapxs_latency_process_incoming_host_aapxs_reply,
            aapxs_latency_get_plugin_proxy,
            aapxs_latency_get_host_proxy,
            nullptr,
            aapxs_latency_get_host_receiver,
            nullptr,
            aapxs_latency_request_flags,
            nullptr,
            nullptr,
            nullptr,
            initializeTypedAAPXSInitiator<LatencyClientAAPXS, LatencyServiceAAPXS>,
            nullptr,
            releaseTypedAAPXSInitiator,
            nullptr
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_latency;
        }
    };
}

#endif //AAP_CORE_LATENCY_AAPXS_H
