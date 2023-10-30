
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
                                                aap::aapxs_recipient_receive_func processIncomingAAPXSRequest,
                                                aap::aapxs_recipient_send_func sendAAPXSReply,
                                                aap::initiator_get_new_request_id_func initiatorGetNewRequestId,
                                                aap::recipient_get_new_request_id_func recipientGetNewRequestId) {
    std::for_each(registry->begin(), registry->end(), [&](AAPXSDefinition* f) {
        // plugin extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, sendAAPXSRequest, processIncomingAAPXSReply, initiatorGetNewRequestId), f->uri);
        // host extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, processIncomingAAPXSRequest, sendAAPXSReply, recipientGetNewRequestId), f->uri);
    });
}

AAPXSInitiatorInstance aap::AAPXSClientDispatcher::populateAAPXSInitiatorInstance(
        AAPXSSerializationContext* serialization,
        aap::aapxs_initiator_send_func sendAAPXSRequest,
        aap::aapxs_initiator_receive_func processIncomingAAPXSReply,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    sendAAPXSRequest,
                                    processIncomingAAPXSReply};
    return instance;
}

AAPXSRecipientInstance
aap::AAPXSClientDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization,
        aap::aapxs_recipient_receive_func processIncomingAapxsRequest,
        aap::aapxs_recipient_send_func sendAapxsReply,
        aap::recipient_get_new_request_id_func getNewRequestId) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    processIncomingAapxsRequest,
                                    sendAapxsReply};
    return instance;
}

// Service setup

void aap::AAPXSServiceDispatcher::setupInstances(aap::AAPXSDefinitionServiceRegistry *registry,
                                                 AAPXSSerializationContext *serialization,
                                                 aap::aapxs_initiator_send_func sendAAPXSRequest,
                                                 aap::aapxs_initiator_receive_func processIncomingAAPXSReply,
                                                 aap::aapxs_recipient_receive_func processIncomingAapxsRequest,
                                                 aap::aapxs_recipient_send_func sendAapxsReply,
                                                 aap::initiator_get_new_request_id_func initiatorGetNewRequestId,
                                                 aap::recipient_get_new_request_id_func recipientGetNewRequestId) {
    std::for_each(registry->begin(), registry->end(), [&](AAPXSDefinition* f) {
        // host extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, sendAAPXSRequest, processIncomingAAPXSReply, initiatorGetNewRequestId), f->uri);
        // plugin extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, processIncomingAapxsRequest, sendAapxsReply, recipientGetNewRequestId), f->uri);
    });
}

AAPXSRecipientInstance
aap::AAPXSServiceDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization,
        aap::aapxs_recipient_receive_func processIncomingAAPXSRequest,
        aap::aapxs_recipient_send_func sendAAPXSReply,
        recipient_get_new_request_id_func getNewRequestId) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    processIncomingAAPXSRequest,
                                    sendAAPXSReply};
    return instance;
}

AAPXSInitiatorInstance
aap::AAPXSServiceDispatcher::populateAAPXSInitiatorInstance(
        AAPXSSerializationContext *serialization,
        aapxs_initiator_send_func sendHostAAPXSRequest,
        aapxs_initiator_receive_func processIncomingHostAAPXSReply,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    sendHostAAPXSRequest,
                                    processIncomingHostAAPXSReply};
    return instance;
}
