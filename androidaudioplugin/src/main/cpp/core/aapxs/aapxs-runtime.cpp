
#include "aap/unstable/aapxs-vnext.h"
#include "aap/core/aapxs/aapxs-runtime.h"

// Client setup

aap::xs::AAPXSClientDispatcher::AAPXSClientDispatcher(AAPXSDefinitionClientRegistry *registry)
        : AAPXSDispatcher(registry->items()->getUridMapping()), registry(registry) {
}

bool aap::xs::AAPXSClientDispatcher::setupInstances(void* hostContext,
                                                    std::function<bool(const char*, AAPXSSerializationContext*)> sharedMemoryAllocatingRequester,
                                                    aapxs_initiator_send_func sendAAPXSRequest,
                                                    aapxs_recipient_send_func sendAAPXSReply,
                                                    initiator_get_new_request_id_func initiatorGetNewRequestId) {
    assert(!already_setup);

    auto items = registry->items();
    if (!std::all_of(items->begin(), items->end(), [&](AAPXSDefinition& f) {
        if (!f.uri)
            return true; // skip
        int32_t urid = registry->items()->getUridMapping()->getUrid(f.uri);
        // allocate SerializationContext
        auto serialization = std::make_unique<AAPXSSerializationContext>();
        serialization->data_capacity = f.data_capacity;
        if (!sharedMemoryAllocatingRequester(f.uri, serialization.get()))
            return false;
        // plugin extensions
        addInitiator(populateAAPXSInitiatorInstance(hostContext, serialization.get(), sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // host extensions
        addRecipient(populateAAPXSRecipientInstance(hostContext, serialization.get(), sendAAPXSReply), f.uri);
        serialization_store[urid] = std::move(serialization);
        return true;
    }))
        return false;
    already_setup = true;
    return true;
}

AAPXSInitiatorInstance aap::xs::AAPXSClientDispatcher::populateAAPXSInitiatorInstance(
        void* hostContext,
        AAPXSSerializationContext* serialization,
        aapxs_initiator_send_func sendAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    hostContext,
                                    serialization,
                                    getNewRequestId,
                                    sendAAPXSRequest};
    return instance;
}

AAPXSRecipientInstance
aap::xs::AAPXSClientDispatcher::populateAAPXSRecipientInstance(
        void* hostContext,
        AAPXSSerializationContext *serialization,
        aapxs_recipient_send_func sendAapxsReply) {
    AAPXSRecipientInstance instance{this,
                                    hostContext,
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

void aap::xs::AAPXSServiceDispatcher::setupInstances(void* hostContext,
                                                     std::function<void(const char*,AAPXSSerializationContext*)> extensionBufferAssigner,
                                                     aapxs_recipient_send_func sendAapxsReply,
                                                     aapxs_initiator_send_func sendAAPXSRequest,
                                                     initiator_get_new_request_id_func initiatorGetNewRequestId) {
    assert(!already_setup);

    auto items = registry->items();
    std::for_each(items->begin(), items->end(), [&](AAPXSDefinition& f) {
        if (!f.uri)
            return; // skip
        int32_t urid = registry->items()->getUridMapping()->getUrid(f.uri);
        // allocate SerializationContext
        auto serialization = std::make_unique<AAPXSSerializationContext>();
        // host extensions
        addInitiator(populateAAPXSInitiatorInstance(hostContext, serialization.get(), sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // plugin extensions
        addRecipient(populateAAPXSRecipientInstance(hostContext, serialization.get(), sendAapxsReply), f.uri);
        extensionBufferAssigner(f.uri, serialization.get());
        serialization_store[urid] = std::move(serialization);
    });
    already_setup = true;
}

AAPXSRecipientInstance
aap::xs::AAPXSServiceDispatcher::populateAAPXSRecipientInstance(
        void* hostContext,
        AAPXSSerializationContext *serialization,
        aapxs_recipient_send_func sendAAPXSReply) {
    AAPXSRecipientInstance instance{this,
                                    hostContext,
                                    serialization,
                                    sendAAPXSReply};
    return instance;
}

AAPXSInitiatorInstance
aap::xs::AAPXSServiceDispatcher::populateAAPXSInitiatorInstance(
        void* hostContext,
        AAPXSSerializationContext *serialization,
        aapxs_initiator_send_func sendHostAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{this,
                                    hostContext,
                                    serialization,
                                    getNewRequestId,
                                    sendHostAAPXSRequest};
    return instance;
}
