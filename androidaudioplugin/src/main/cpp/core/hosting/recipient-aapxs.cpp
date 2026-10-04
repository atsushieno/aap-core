#include "recipient-aapxs.h"
#include "aap/core/realtime.h"
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>

namespace {
std::mutex registryMutex;
std::unordered_map<AAPXSRequestContext*, std::weak_ptr<aap::xs::DeferredAAPXSReply>> registry;
}

std::shared_ptr<aap::xs::DeferredAAPXSReply> aap::xs::retainAAPXSReply(AAPXSRequestContext* request) {
    if (!request || RealtimeScope::isActive()) return {};
    std::lock_guard<std::mutex> lock(registryMutex);
    auto found = registry.find(request);
    return found == registry.end() ? nullptr : found->second.lock();
}

struct aap::internal::RecipientRequestStore::State : std::enable_shared_from_this<State> {
    struct Reply : xs::DeferredAAPXSReply {
        std::shared_ptr<State> state;
        std::string uri;
        std::vector<uint8_t> data;
        AAPXSSerializationContext serialization;
        AAPXSRequestContext context;
        Reply(std::shared_ptr<State> state, const AAPXSRequestContext& input, size_t capacity)
            : state(std::move(state)), uri(input.uri ? input.uri : ""), data(capacity),
              serialization{data.data(), input.serialization->data_size, capacity}, context(input) {
            if (serialization.data_size) memcpy(data.data(), input.serialization->data, serialization.data_size);
            context.uri = uri.c_str();
            context.serialization = &serialization;
        }
        ~Reply() override {
            std::lock_guard<std::mutex> lock(registryMutex);
            registry.erase(&context);
        }
        AAPXSRequestContext& request() override { return context; }
        bool complete() override {
            if (RealtimeScope::isActive()) return false;
            std::lock_guard<std::mutex> lock(state->mutex);
            auto found = state->pending.find(&context);
            if (state->closed || found == state->pending.end() ||
                context.serialization != &serialization || serialization.data != data.data() ||
                serialization.data_capacity != data.size() || serialization.data_size > data.size()) return false;
            if (!state->sender || !state->sender(context)) return false;
            // The caller holds a shared lease, including synchronous handler dispatch.
            state->pending.erase(found);
            return true;
        }
    };
    std::mutex mutex;
    bool closed{false};
    Sender sender;
    // Dispatch and explicitly retained handles own the storage. An extension
    // abandoning its handle must not leave a buffer pinned until teardown.
    std::unordered_map<AAPXSRequestContext*, std::weak_ptr<Reply>> pending;
};

aap::internal::RecipientRequestStore::RecipientRequestStore() : state(std::make_shared<State>()) {}
aap::internal::RecipientRequestStore::~RecipientRequestStore() { close(); }
void aap::internal::RecipientRequestStore::setSender(Sender sender) {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->closed) state->sender = std::move(sender);
}
std::shared_ptr<aap::xs::DeferredAAPXSReply> aap::internal::RecipientRequestStore::create(
        const AAPXSRequestContext& request, size_t capacity) {
    if (RealtimeScope::isActive() || !request.serialization || request.serialization->data_size > capacity ||
        (request.serialization->data_size && !request.serialization->data)) return {};
    std::lock_guard<std::mutex> lock(state->mutex);
    for (auto it = state->pending.begin(); it != state->pending.end();) {
        if (it->second.expired()) it = state->pending.erase(it);
        else ++it;
    }
    if (state->closed || state->pending.size() >= 255) return {};
    auto reply = std::make_shared<State::Reply>(state, request, capacity);
    state->pending.emplace(&reply->context, reply);
    {
        std::lock_guard<std::mutex> registryLock(registryMutex);
        registry.emplace(&reply->context, reply);
    }
    return reply;
}
void aap::internal::RecipientRequestStore::close() {
    std::lock_guard<std::mutex> lock(state->mutex);
    state->closed = true;
    state->sender = {};
    state->pending.clear();
}
