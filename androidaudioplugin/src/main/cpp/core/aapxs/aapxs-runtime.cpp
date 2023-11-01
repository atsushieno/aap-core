
#include "aap/unstable/aapxs-vnext.h"
#include "aap/core/aapxs/aapxs-runtime.h"

aap::xs::AAPXSClientDispatcher::AAPXSClientDispatcher(
        AAPXSDefinitionClientRegistry *registry) /*: registry(registry)*/ {
}

aap::xs::AAPXSServiceDispatcher::AAPXSServiceDispatcher(
        AAPXSDefinitionServiceRegistry *registry) {
}

// Client setup

void aap::xs::AAPXSClientDispatcher::setupInstances(AAPXSDefinitionClientRegistry *registry,
                                                AAPXSSerializationContext *serialization,
                                                aapxs_initiator_send_func sendAAPXSRequest,
                                                aapxs_recipient_send_func sendAAPXSReply,
                                                initiator_get_new_request_id_func initiatorGetNewRequestId) {
    auto items = registry->items();
    std::for_each(items->begin(), items->end(), [&](AAPXSDefinition& f) {
        // plugin extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // host extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, sendAAPXSReply), f.uri);
    });
}

AAPXSInitiatorInstance aap::xs::AAPXSClientDispatcher::populateAAPXSInitiatorInstance(
        AAPXSSerializationContext* serialization,
        aapxs_initiator_send_func sendAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    sendAAPXSRequest};
    return instance;
}

AAPXSRecipientInstance
aap::xs::AAPXSClientDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization,
        aapxs_recipient_send_func sendAapxsReply) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    sendAapxsReply};
    return instance;
}

// Service setup

void aap::xs::AAPXSServiceDispatcher::setupInstances(AAPXSDefinitionServiceRegistry *registry,
                                                 AAPXSSerializationContext *serialization,
                                                 aapxs_recipient_send_func sendAapxsReply,
                                                 aapxs_initiator_send_func sendAAPXSRequest,
                                                 initiator_get_new_request_id_func initiatorGetNewRequestId) {
    auto items = registry->items();
    std::for_each(items->begin(), items->end(), [&](AAPXSDefinition& f) {
        // host extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // plugin extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, sendAapxsReply), f.uri);
    });
}

AAPXSRecipientInstance
aap::xs::AAPXSServiceDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization,
        aapxs_recipient_send_func sendAAPXSReply) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    sendAAPXSReply};
    return instance;
}

AAPXSInitiatorInstance
aap::xs::AAPXSServiceDispatcher::populateAAPXSInitiatorInstance(
        AAPXSSerializationContext *serialization,
        aapxs_initiator_send_func sendHostAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    sendHostAAPXSRequest};
    return instance;
}

void aap::xs::TypedClientAAPXS::getVoidCallback(void* callbackContext, AndroidAudioPlugin* plugin, int32_t requestId) {
    auto callbackData = (WithPromise<TypedClientAAPXS, int32_t>*) callbackContext;
    callbackData->promise->set_value(0); // dummy result
}

void aap::xs::TypedClientAAPXS::callVoidFunctionSynchronously(int32_t opcode) {
    // FIXME: use spinlock instead of std::promise and std::future, as getPresetCount() and getPresetIndex() must be RT_SAFE.
    std::promise<int32_t> promise{};
    uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
    auto future = promise.get_future();
    WithPromise<TypedClientAAPXS, int32_t> callbackData{this, &promise};
    AAPXSRequestContext request{getVoidCallback, &callbackData, serialization, uri, requestId, opcode};

    aapxs_instance->send_aapxs_request(aapxs_instance, &request);

    future.wait();
}
