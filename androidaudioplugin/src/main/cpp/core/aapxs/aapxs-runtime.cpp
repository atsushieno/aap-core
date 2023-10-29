
#include "aap/unstable/aapxs-vnext.h"
#include "aap/core/aapxs/aapxs-runtime.h"

aap::AAPXSClientDispatcher::AAPXSClientDispatcher(
        aap::AAPXSClientFeatureRegistry *registry) /*: registry(registry)*/ {
}

aap::AAPXSServiceDispatcher::AAPXSServiceDispatcher(
        aap::AAPXSServiceFeatureRegistry *registry) {
}

// Client setup
void aap::AAPXSClientDispatcher::setupInstances(aap::AAPXSClientFeatureRegistry *registry,
                                                AAPXSSerializationContext *serialization,
                                                initiator_get_new_request_id_func initiatorGetNewRequestId,
                                                recipient_get_new_request_id_func recipientGetNewRequestId) {
    std::for_each(registry->begin(), registry->end(), [&](AAPXSFeatureVNext* f) {
        // plugin extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, initiatorGetNewRequestId), f->uri);
        // host extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, recipientGetNewRequestId), f->uri);
    });
}

AAPXSInitiatorInstance aap::AAPXSClientDispatcher::populateAAPXSInitiatorInstance(AAPXSSerializationContext* serialization, initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    staticSendAAPXSRequest,
                                    staticProcessIncomingAAPXSReply};
    return instance;
}

AAPXSRecipientInstance aap::AAPXSClientDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization, recipient_get_new_request_id_func getNewRequestId) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    staticProcessIncomingAAPXSRequest,
                                    staticSendAAPXSReply};
    return instance;
}

// Client send/receive
void
aap::AAPXSClientDispatcher::sendExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, newRequestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_request(&aapxs, &context);
}

void
aap::AAPXSClientDispatcher::processExtensionReply(const char *uri, int32_t opcode, void* data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_reply(&aapxs, &context);
}

void
aap::AAPXSClientDispatcher::processHostExtensionRequest(const char *uri, int32_t opcode, void* data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_request(&aapxs, &context);
}

void
aap::AAPXSClientDispatcher::sendHostExtensionReply(const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_reply(&aapxs, &context);
}

// Service send/receive

// setup
void aap::AAPXSServiceDispatcher::setupInstances(aap::AAPXSServiceFeatureRegistry *registry,
                                                 AAPXSSerializationContext *serialization,
                                                 initiator_get_new_request_id_func initiatorGetNewRequestId,
                                                 recipient_get_new_request_id_func recipientGetNewRequestId) {
    std::for_each(registry->begin(), registry->end(), [&](AAPXSFeatureVNext* f) {
        // host extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization, initiatorGetNewRequestId), f->uri);
        // plugin extensions
        addRecipient(populateAAPXSRecipientInstance(serialization, recipientGetNewRequestId), f->uri);
    });
}


AAPXSRecipientInstance
aap::AAPXSServiceDispatcher::populateAAPXSRecipientInstance(
        AAPXSSerializationContext *serialization, recipient_get_new_request_id_func getNewRequestId) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    staticProcessIncomingAAPXSRequest,
                                    staticSendAAPXSReply};
    return instance;
}

AAPXSInitiatorInstance
aap::AAPXSServiceDispatcher::populateAAPXSInitiatorInstance(
        AAPXSSerializationContext *serialization, initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    getNewRequestId,
                                    staticSendHostAAPXSRequest,
                                    staticProcessIncomingHostAAPXSReply};
    return instance;
}

// send/receive
void
aap::AAPXSServiceDispatcher::processExtensionRequest(const char *uri, int32_t opcode, uint32_t requestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_request(&aapxs, &context);
}

void
aap::AAPXSServiceDispatcher::sendExtensionReply(const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_reply(&aapxs, &context);
}

void
aap::AAPXSServiceDispatcher::sendHostExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, newRequestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_request(&aapxs, &context);
}

void aap::AAPXSServiceDispatcher::processHostExtensionReply(const char *uri, int32_t opcode, uint32_t requestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_reply(&aapxs, &context);
}
