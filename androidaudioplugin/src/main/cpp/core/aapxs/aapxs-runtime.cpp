
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
