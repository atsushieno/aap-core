
#include "aap/aapxs.h"
#include "aap/core/aapxs/aapxs-hosting-runtime.h"
#include "aap/unstable/utility.h"
#include "../hosting/aapxs-shared-transport.h"

void aap::xs::AAPXSDispatcher::refreshTransport() { if (shared_transport) shared_transport->refreshClient(); }
uint32_t aap::xs::AAPXSDispatcher::getTransportCapabilities() const { return shared_transport ? shared_transport->getCapabilities() : 0; }

// Client setup

aap::xs::AAPXSClientDispatcher::AAPXSClientDispatcher(AAPXSDefinitionRegistry *registry)
        : AAPXSDispatcher(registry->getUridMapping()), registry(registry) {
    shared_transport = std::make_shared<internal::SharedAAPXSTransport>();
}

bool aap::xs::AAPXSClientDispatcher::setupInstances(void* hostContext,
                                                    std::function<bool(const char*, AAPXSSerializationContext*)> sharedMemoryAllocatingRequester,
                                                    aapxs_initiator_send_func sendAAPXSRequest,
                                                    aapxs_recipient_send_func sendAAPXSReply,
                                                    initiator_get_new_request_id_func initiatorGetNewRequestId) {
    if (already_setup) {
        AAP_ASSERT_FALSE; // should not reach here
        return false;
    }

    AAPXSSerializationContext descriptor{nullptr, 0, internal::AAPXS_TRANSPORT_DESCRIPTOR_SIZE};
    if (!sharedMemoryAllocatingRequester(internal::AAPXS_TRANSPORT_URI, &descriptor)) return false;
    shared_transport->setDescriptor(descriptor, true);
    if (!std::all_of(registry->begin(), registry->end(), [&](AAPXSDefinition& f) {
        if (!f.uri)
            return true; // skip
        int32_t urid = registry->getUridMapping()->getUrid(f.uri);
        // allocate SerializationContext
        auto serialization = std::make_unique<AAPXSSerializationContext>();
        serialization->data_capacity = f.data_capacity ? f.data_capacity + internal::AAPXS_TRANSPORT_BLOCK_PREFIX : 0;
        if (!sharedMemoryAllocatingRequester(f.uri, serialization.get()))
            return false;
        auto hostSerialization = std::make_unique<AAPXSSerializationContext>();
        hostSerialization->data_capacity = serialization->data_capacity;
        auto hostUri = internal::aapxsHostDirectionUri(f.uri);
        if (f.data_capacity && !sharedMemoryAllocatingRequester(hostUri.c_str(), hostSerialization.get())) return false;
        shared_transport->add(serialization.get(), hostSerialization.get(), f.data_capacity,
                serialization->data, serialization->data_capacity, hostSerialization->data, hostSerialization->data_capacity);
        // plugin extensions
        addInitiator(populateAAPXSInitiatorInstance(hostContext, serialization.get(), urid, sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // host extensions
        addRecipient(populateAAPXSRecipientInstance(hostContext, hostSerialization.get(), sendAAPXSReply), f.uri);
        serialization_store[urid] = std::move(serialization);
        host_serialization_store[urid] = std::move(hostSerialization);
        return true;
    }))
        return false;
    already_setup = true;
    return true;
}

AAPXSInitiatorInstance aap::xs::AAPXSClientDispatcher::populateAAPXSInitiatorInstance(
        void* hostContext,
        AAPXSSerializationContext* serialization,
        uint8_t urid,
        aapxs_initiator_send_func sendAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{nullptr,
                                    hostContext,
                                    serialization,
                                    urid,
                                    getNewRequestId,
                                    sendAAPXSRequest};
    return instance;
}

AAPXSRecipientInstance
aap::xs::AAPXSClientDispatcher::populateAAPXSRecipientInstance(
        void* hostContext,
        AAPXSSerializationContext *serialization,
        aapxs_recipient_send_func sendAapxsReply) {
    AAPXSRecipientInstance instance{nullptr,
                                    hostContext,
                                    serialization,
                                    sendAapxsReply};
    return instance;
}

AAPXSSerializationContext *aap::xs::AAPXSClientDispatcher::getSerialization(const char *uri) {
    auto& shm = serialization_store[registry->getUridMapping()->getUrid(uri)];
    return shm ? shm.get() : nullptr;
}

// Service setup

aap::xs::AAPXSServiceDispatcher::AAPXSServiceDispatcher(AAPXSDefinitionRegistry *registry)
        : AAPXSDispatcher(registry->getUridMapping()), registry(registry) {
    shared_transport = std::make_shared<internal::SharedAAPXSTransport>();
}

void aap::xs::AAPXSServiceDispatcher::setupInstances(void* hostContext,
                                                     std::function<void(const char*,AAPXSSerializationContext*)> extensionBufferAssigner,
                                                     aapxs_recipient_send_func sendAapxsReply,
                                                     aapxs_initiator_send_func sendAAPXSRequest,
                                                     initiator_get_new_request_id_func initiatorGetNewRequestId) {
    if (already_setup) {
        AAP_ASSERT_FALSE; // should not reach here
        return;
    }

    AAPXSSerializationContext descriptor{};
    extensionBufferAssigner(internal::AAPXS_TRANSPORT_URI, &descriptor);
    shared_transport->setDescriptor(descriptor, false);
    std::for_each(registry->begin(), registry->end(), [&](AAPXSDefinition& f) {
        if (!f.uri)
            return; // skip
        int32_t urid = registry->getUridMapping()->getUrid(f.uri);
        // allocate SerializationContext
        auto serialization = std::make_unique<AAPXSSerializationContext>();
        auto hostSerialization = std::make_unique<AAPXSSerializationContext>();
        extensionBufferAssigner(f.uri, serialization.get());
        auto hostUri = internal::aapxsHostDirectionUri(f.uri);
        extensionBufferAssigner(hostUri.c_str(), hostSerialization.get());
        shared_transport->add(serialization.get(), hostSerialization.get(), f.data_capacity,
                serialization->data, serialization->data_capacity, hostSerialization->data, hostSerialization->data_capacity);
        // host extensions
        addInitiator(populateAAPXSInitiatorInstance(hostContext, hostSerialization.get(), urid, sendAAPXSRequest, initiatorGetNewRequestId), f.uri);
        // plugin extensions
        addRecipient(populateAAPXSRecipientInstance(hostContext, serialization.get(), sendAapxsReply), f.uri);
        serialization_store[urid] = std::move(serialization);
        host_serialization_store[urid] = std::move(hostSerialization);
    });
    shared_transport->acceptService();
    already_setup = true;
}

AAPXSRecipientInstance
aap::xs::AAPXSServiceDispatcher::populateAAPXSRecipientInstance(
        void* hostContext,
        AAPXSSerializationContext *serialization,
        aapxs_recipient_send_func sendAAPXSReply) {
    AAPXSRecipientInstance instance{nullptr,
                                    hostContext,
                                    serialization,
                                    sendAAPXSReply};
    return instance;
}

AAPXSInitiatorInstance
aap::xs::AAPXSServiceDispatcher::populateAAPXSInitiatorInstance(
        void* hostContext,
        AAPXSSerializationContext *serialization,
        uint8_t urid,
        aapxs_initiator_send_func sendHostAAPXSRequest,
        initiator_get_new_request_id_func getNewRequestId) {
    AAPXSInitiatorInstance instance{nullptr,
                                    hostContext,
                                    serialization,
                                    urid,
                                    getNewRequestId,
                                    sendHostAAPXSRequest};
    return instance;
}
