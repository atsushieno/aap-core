#ifndef AAP_CORE_TAIL_AAPXS_H
#define AAP_CORE_TAIL_AAPXS_H

#include <functional>
#include "aap/aapxs.h"
#include "../../ext/tail.h"
#include "typed-aapxs.h"

// plugin extension opcodes

// Request: none. Reply: double tail length in seconds.
const int32_t OPCODE_GET_TAIL_LENGTH = 1;

// host extension opcodes
const int32_t OPCODE_NOTIFY_TAIL_LENGTH_CHANGED = -1;

const int32_t TAIL_SHARED_MEMORY_SIZE = sizeof(double);

namespace aap::xs {

    class TailClientAAPXS : public TypedAAPXS {
        static double staticGetTailLength(aap_tail_extension_t* ext, AndroidAudioPlugin*) {
            auto result = ((TailClientAAPXS*) ext->aapxs_context)->getTailLength();
            return result.isOk() ? result.value : 0.0;
        }
        aap_tail_extension_t as_plugin_extension{this, staticGetTailLength};

    public:
        TailClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_TAIL_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // Blocking-sync. Not for processing threads; cache the value and refresh it on notification.
        Result<double> getTailLength();
        // Asynchronous. Returns the request id; the callback fires exactly once on reply/timeout/death.
        int32_t getTailLengthAsync(std::function<void(Result<double>)> callback);

        aap_tail_extension_t* asPluginExtension() { return &as_plugin_extension; }
    };

    class TailServiceAAPXS : public TypedAAPXS {
        static void staticNotifyTailLengthChanged(aap_tail_host_extension_t* ext, AndroidAudioPluginHost*) {
            ((TailServiceAAPXS*) ext->aapxs_context)->notifyTailLengthChanged();
        }
        aap_tail_host_extension_t as_host_extension{this, staticNotifyTailLengthChanged};

    public:
        TailServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_TAIL_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyTailLengthChanged();

        aap_tail_host_extension_t* asHostExtension() { return &as_host_extension; }
    };

    class AAPXSDefinition_Tail : public AAPXSDefinitionWrapper {

        static void aapxs_tail_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_tail_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_tail_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_tail_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        static AAPXSExtensionClientProxy aapxs_tail_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);
        static AAPXSExtensionServiceProxy aapxs_tail_get_host_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);
        static AAPXSExtensionHostReceiver aapxs_tail_get_host_receiver(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host);

        static uint32_t aapxs_tail_request_flags(AAPXSDefinition*, bool, int32_t);
        static void aapxs_tail_state_changed(AAPXSDefinition*, AAPXSRecipientInstance*, AndroidAudioPlugin*);
        static void aapxs_tail_release_plugin_context(AAPXSDefinition*, void*);
        static bool aapxs_tail_initialize_recipient(AAPXSDefinition*, AAPXSRecipientInstance*, bool host);
        static void aapxs_tail_release_recipient(AAPXSDefinition*, AAPXSRecipientInstance*, bool host);

        static void* aapxs_tail_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            auto* client = (TailClientAAPXS*) proxy->aapxs_context;
            return client ? client->asPluginExtension() : nullptr;
        }
        static void* aapxs_tail_as_host_extension(AAPXSExtensionServiceProxy* proxy) {
            auto* service = (TailServiceAAPXS*) proxy->aapxs_context;
            return service ? service->asHostExtension() : nullptr;
        }
        static void* aapxs_tail_as_host_receiver(AAPXSExtensionHostReceiver* receiver);

        // The query reads an atomic snapshot, so it matches RT_SAFE in ext/tail.h.
        // Host callbacks are always treated as RT-unsafe by the runtime.
        static bool aapxs_tail_is_command_rt_safe(struct AAPXSDefinition*, bool isHostExtension, int32_t opcode) {
            return !isHostExtension && opcode == OPCODE_GET_TAIL_LENGTH;
        }

        AAPXSDefinition aapxs_tail{
            this,
            AAP_TAIL_EXTENSION_URI,
            TAIL_SHARED_MEMORY_SIZE,
            aapxs_tail_process_incoming_plugin_aapxs_request,
            aapxs_tail_process_incoming_host_aapxs_request,
            aapxs_tail_process_incoming_plugin_aapxs_reply,
            aapxs_tail_process_incoming_host_aapxs_reply,
            aapxs_tail_get_plugin_proxy,
            aapxs_tail_get_host_proxy,
            aapxs_tail_is_command_rt_safe,
            aapxs_tail_get_host_receiver,
            nullptr,
            aapxs_tail_request_flags,
            aapxs_tail_state_changed,
            aapxs_tail_release_plugin_context,
            nullptr,
            initializeTypedAAPXSInitiator<TailClientAAPXS, TailServiceAAPXS>,
            aapxs_tail_initialize_recipient,
            releaseTypedAAPXSInitiator,
            aapxs_tail_release_recipient
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_tail;
        }
    };
}

#endif //AAP_CORE_TAIL_AAPXS_H
