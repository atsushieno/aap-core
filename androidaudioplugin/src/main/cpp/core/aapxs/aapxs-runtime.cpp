
#include "aap/unstable/aapxs-vnext.h"
#include "aap/core/aapxs/aapxs-runtime.h"

aap::AAPXSClientDispatcher::AAPXSClientDispatcher(
        aap::AAPXSDefinitionClientRegistry *registry) /*: registry(registry)*/ {
}

aap::AAPXSServiceDispatcher::AAPXSServiceDispatcher(
        aap::AAPXSDefinitionServiceRegistry *registry) {
}

// Client setup

void aap::AAPXSClientDispatcher::setupInstances(aap::AAPXSDefinitionClientRegistry *registry,
                                                AAPXSSerializationContext *serialization,
                                                aap::aapxs_initiator_send_func sendAAPXSRequest,
                                                aap::aapxs_initiator_receive_func processIncomingAAPXSReply,
                                                aap::aapxs_recipient_send_func sendAAPXSReply,
                                                aap::initiator_get_new_request_id_func initiatorGetNewRequestId) {
    std::for_each(registry->begin(), registry->end(), [&](AAPXSDefinition& f) {
        // plugin extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // host extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, sendAAPXSReply), f.uri);
    });
}

AAPXSInitiatorInstance aap::AAPXSClientDispatcher::populateAAPXSInitiatorInstance(
        AAPXSSerializationContext* serialization,
        aap::aapxs_initiator_send_func sendAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    sendAAPXSRequest};
    return instance;
}

AAPXSRecipientInstance
aap::AAPXSClientDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization,
        aap::aapxs_recipient_send_func sendAapxsReply) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    sendAapxsReply};
    return instance;
}

// Service setup

void aap::AAPXSServiceDispatcher::setupInstances(aap::AAPXSDefinitionServiceRegistry *registry,
                                                 AAPXSSerializationContext *serialization,
                                                 aapxs_recipient_send_func sendAapxsReply,
                                                 aapxs_initiator_send_func sendAAPXSRequest,
                                                 aap::initiator_get_new_request_id_func initiatorGetNewRequestId) {
    std::for_each(registry->begin(), registry->end(), [&](AAPXSDefinition& f) {
        // host extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // plugin extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, sendAapxsReply), f.uri);
    });
}

AAPXSRecipientInstance
aap::AAPXSServiceDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization,
        aap::aapxs_recipient_send_func sendAAPXSReply) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    sendAAPXSReply};
    return instance;
}

AAPXSInitiatorInstance
aap::AAPXSServiceDispatcher::populateAAPXSInitiatorInstance(
        AAPXSSerializationContext *serialization,
        aapxs_initiator_send_func sendHostAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    sendHostAAPXSRequest};
    return instance;
}
