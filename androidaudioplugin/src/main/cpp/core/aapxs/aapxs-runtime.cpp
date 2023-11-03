
#include "aap/unstable/aapxs-vnext.h"
#include "aap/core/aapxs/aapxs-runtime.h"

// Client setup

aap::xs::AAPXSClientDispatcher::AAPXSClientDispatcher(AAPXSDefinitionClientRegistry *registry)
        : AAPXSDispatcher(registry->items()->getUridMapping()), registry(registry) {
}

bool aap::xs::AAPXSClientDispatcher::setupInstances(std::function<bool(const char*, AAPXSSerializationContext*)> sharedMemoryAllocatingRequester,
                                                    aapxs_initiator_send_func sendAAPXSRequest,
                                                    aapxs_recipient_send_func sendAAPXSReply,
                                                    initiator_get_new_request_id_func initiatorGetNewRequestId) {
    auto items = registry->items();
    if (!std::all_of(items->begin(), items->end(), [&](AAPXSDefinition& f) {
        int32_t urid = registry->items()->getUridMapping()->getUrid(f.uri);
        // allocate SerializationContext
        auto serialization = std::make_unique<AAPXSSerializationContext>();
        if (!sharedMemoryAllocatingRequester(f.uri, serialization.get()))
            return false;
        // plugin extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization.get(), sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // host extensions
        addRecipient(populateAAPXSRecipientInstance(serialization.get(), sendAAPXSReply), f.uri);
        serialization_store[urid] = std::move(serialization);
        return true;
    }))
        return false;
    return true;
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

AAPXSSerializationContext *aap::xs::AAPXSClientDispatcher::getSerialization(const char *uri) {
    auto& shm = serialization_store[registry->items()->getUridMapping()->getUrid(uri)];
    return shm ? shm.get() : nullptr;
}

// Service setup

aap::xs::AAPXSServiceDispatcher::AAPXSServiceDispatcher(AAPXSDefinitionServiceRegistry *registry)
        : AAPXSDispatcher(registry->items()->getUridMapping()), registry(registry) {
}

void aap::xs::AAPXSServiceDispatcher::setupInstances(aapxs_recipient_send_func sendAapxsReply,
                                                     aapxs_initiator_send_func sendAAPXSRequest,
                                                     initiator_get_new_request_id_func initiatorGetNewRequestId) {
    auto items = registry->items();
    std::for_each(items->begin(), items->end(), [&](AAPXSDefinition& f) {
        int32_t urid = registry->items()->getUridMapping()->getUrid(f.uri);
        // allocate SerializationContext
        auto serialization = std::make_unique<AAPXSSerializationContext>();
        serialization_store[urid] = std::move(serialization);
        // host extensions
        addInitiator(populateAAPXSInitiatorInstance(serialization.get(), sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // plugin extensions
        addRecipient(populateAAPXSRecipientInstance(serialization.get(), sendAapxsReply), f.uri);
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
