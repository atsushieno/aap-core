
#include "aap/unstable/aapxs-vnext.h"
#include "aap/core/aapxs/aapxs-runtime.h"

aap::AAPClientDispatcher::AAPClientDispatcher(
        aap::AAPXSClientFeatureRegistry *registry) /*: registry(registry)*/ {
}

aap::AAPServiceDispatcher::AAPServiceDispatcher(
        aap::AAPXSServiceFeatureRegistry *registry) {
}

// Client send/receive
void
aap::AAPClientDispatcher::sendExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, newRequestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_request(&aapxs, &context);
}

void
aap::AAPClientDispatcher::processExtensionReply(const char *uri, int32_t opcode, void* data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_reply(&aapxs, &context);
}

void
aap::AAPClientDispatcher::processHostExtensionRequest(const char *uri, int32_t opcode, void* data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_request(&aapxs, &context);
}

void
aap::AAPClientDispatcher::sendHostExtensionReply(const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_reply(&aapxs, &context);
}

// Service send/receive

// send/receive
void
aap::AAPServiceDispatcher::processExtensionRequest(const char *uri, int32_t opcode, uint32_t requestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_request(&aapxs, &context);
}

void
aap::AAPServiceDispatcher::sendExtensionReply(const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t requestId) {
    auto aapxs = getPluginAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_reply(&aapxs, &context);
}

void
aap::AAPServiceDispatcher::sendHostExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, newRequestId, opcode};
    assert(aapxs.serialization->data_capacity >= dataSize);
    memcpy(aapxs.serialization->data, data, dataSize);
    aapxs.serialization->data_size = dataSize;
    aapxs.send_aapxs_request(&aapxs, &context);
}

void aap::AAPServiceDispatcher::processHostExtensionReply(const char *uri, int32_t opcode, uint32_t requestId) {
    auto aapxs = getHostAAPXSByUri(uri);
    AAPXSRequestContext context{nullptr, nullptr, aapxs.serialization, uri, requestId, opcode};
    aapxs.process_incoming_aapxs_reply(&aapxs, &context);
}
