#include "aapxs-transport.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <future>
#include <map>
#include "aap/unstable/utility.h"
#include "aap/core/aap_midi2_helper.h"
#include "aap/ext/midi.h"
#include "../include_cmidi2.h"

namespace aap::internal {

struct AAPXSBinderChannel::Pending {
    std::shared_ptr<AAPXSBinderChannel> channel;
    AAPXSRequestContext request;
    // set for requests without callback, whose sender waits for the result.
    std::shared_ptr<std::promise<std::string>> sync{};
};

namespace {
void deliver(const AAPXSRequestContext& request, const char* error, void* pluginOrHost) {
    if (error && request.error_callback)
        request.error_callback(request.callback_user_data, pluginOrHost, error);
    else if (request.callback)
        request.callback(request.callback_user_data, pluginOrHost);
}
}

bool AAPXSBinderChannel::send(AAPXSRequestContext* request) {
    auto pending = std::make_shared<Pending>();
    pending->channel = shared_from_this();
    pending->request = *request;
    std::future<std::string> syncResult;
    if (!request->callback) {
        pending->sync = std::make_shared<std::promise<std::string>>();
        syncResult = pending->sync->get_future();
    }
    {
        std::unique_lock<std::mutex> lock(mutex);
        if (pending->sync)
            idle.wait(lock, [this] { return !busy; });
        else if (busy) {
            queue.push_back(pending);
            return true;
        }
        busy = true;
        current = pending;
    }
    if (!dispatch(pending)) {
        sendNext();
        return false;
    }
    if (pending->sync) {
        syncResult.wait();
        return false;
    }
    return true;
}

bool AAPXSBinderChannel::dispatch(const std::shared_ptr<Pending>& pending) {
    auto source = pending->request.serialization;
    size_t size = source ? source->data_size : 0;
    if (size > shared_block->data_capacity)
        return false;
    if (size > 0 && source != shared_block)
        memcpy(shared_block->data, source->data, size);
    shared_block->data_size = size;

    AAPXSRequestContext routed = pending->request;
    routed.serialization = shared_block;
    routed.callback = onCompleted;
    routed.error_callback = onFailed;
    auto holder = new std::shared_ptr<Pending>(pending);
    routed.callback_user_data = holder;
    if (transmit(routed))
        return true;
    delete holder;
    return false;
}

void AAPXSBinderChannel::onCompleted(void* context, void* pluginOrHost) {
    auto holder = (std::shared_ptr<Pending>*) context;
    auto pending = *holder;
    delete holder;
    pending->channel->complete(pending.get(), nullptr, pluginOrHost);
}

void AAPXSBinderChannel::onFailed(void* context, void* pluginOrHost, const char* error) {
    auto holder = (std::shared_ptr<Pending>*) context;
    auto pending = *holder;
    delete holder;
    pending->channel->complete(pending.get(), error ? error : "error", pluginOrHost);
}

void AAPXSBinderChannel::complete(Pending* pending, const char* error, void* pluginOrHost) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (current.get() != pending)
            return; // aborted; the request buffer may be gone already
        auto target = pending->request.serialization;
        if (!error && target && target != shared_block) {
            auto size = std::min(target->data_capacity, shared_block->data_capacity);
            memcpy(target->data, shared_block->data, size);
            target->data_size = size;
        }
    }
    sendNext();
    if (pending->sync)
        pending->sync->set_value(error ? error : "");
    else
        deliver(pending->request, error, pluginOrHost);
}

void AAPXSBinderChannel::sendNext() {
    while (true) {
        std::shared_ptr<Pending> next;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (queue.empty()) {
                busy = false;
                current.reset();
                idle.notify_all();
                return;
            }
            next = queue.front();
            queue.pop_front();
            current = next;
        }
        if (dispatch(next))
            return;
        deliver(next->request, "request could not be sent", nullptr);
    }
}

void AAPXSBinderChannel::abort(const char* error, void* pluginOrHost) {
    std::shared_ptr<Pending> inFlight;
    std::deque<std::shared_ptr<Pending>> dropped;
    {
        std::lock_guard<std::mutex> lock(mutex);
        inFlight = std::move(current);
        current.reset();
        dropped = std::move(queue);
        queue.clear();
        busy = false;
        idle.notify_all();
    }
    if (inFlight) {
        if (inFlight->sync)
            inFlight->sync->set_value(error);
        else
            deliver(inFlight->request, error, pluginOrHost);
    }
    for (auto& d : dropped)
        deliver(d->request, error, pluginOrHost);
}

// ---- registry

namespace {
struct Registry {
    std::mutex mutex{};
    std::map<const void*, std::map<AAPXSSerializationContext*, std::shared_ptr<AAPXSBinderChannel>>> channels{};
};

Registry& registry() {
    static auto* r = new Registry(); // never destroyed; completions may arrive during process teardown
    return *r;
}
}

std::shared_ptr<AAPXSBinderChannel> getAAPXSBinderChannel(const void* owner,
                                                          AAPXSSerializationContext* sharedBlock,
                                                          const std::function<AAPXSBinderChannel::Transmit()>& createTransmit) {
    auto& r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    auto& channel = r.channels[owner][sharedBlock];
    if (!channel)
        channel = std::make_shared<AAPXSBinderChannel>(sharedBlock, createTransmit());
    return channel;
}

void abortAAPXSBinderChannels(const void* owner, const char* error, void* pluginOrHost) {
    std::vector<std::shared_ptr<AAPXSBinderChannel>> targets;
    {
        auto& r = registry();
        std::lock_guard<std::mutex> lock(r.mutex);
        auto it = r.channels.find(owner);
        if (it == r.channels.end())
            return;
        for (auto& kv : it->second)
            targets.push_back(kv.second);
    }
    for (auto& channel : targets)
        channel->abort(error, pluginOrHost);
}

void releaseAAPXSBinderChannels(const void* owner) {
    abortAAPXSBinderChannels(owner, "AAPXS owner destroyed", nullptr);
    auto& r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.channels.erase(owner);
}

// ---- SysEx8 pending requests

namespace sysex8 {
namespace {
struct Entry {
    bool used{false};
    uint32_t request_id{0};
    aapxs_completion_callback callback{nullptr};
    aapxs_error_callback error_callback{nullptr};
    void* callback_user_data{nullptr};
    AAPXSSerializationContext* buffer{nullptr};
};

constexpr size_t MAX_PENDING_REQUESTS = 1024;

struct Table {
    NanoSleepLock lock{};
    std::array<Entry, MAX_PENDING_REQUESTS> entries{};
};

Table& table() {
    static auto* t = new Table();
    return *t;
}

Entry take(Entry* entry) {
    const std::lock_guard<NanoSleepLock> guard{table().lock};
    Entry ret = *entry;
    *entry = Entry{};
    return ret;
}

void onReply(void* context, void* pluginOrHost) {
    auto entry = take((Entry*) context);
    if (entry.callback)
        entry.callback(entry.callback_user_data, pluginOrHost);
}

void onError(void* context, void* pluginOrHost, const char* error) {
    auto entry = take((Entry*) context);
    if (entry.error_callback)
        entry.error_callback(entry.callback_user_data, pluginOrHost, error);
    else if (entry.callback)
        entry.callback(entry.callback_user_data, pluginOrHost);
}
}

bool registerRequest(const AAPXSRequestContext& request, AAPXSRequestContext& routed) {
    auto& t = table();
    const std::lock_guard<NanoSleepLock> guard{t.lock};
    for (size_t i = 0; i < MAX_PENDING_REQUESTS; i++) {
        auto& entry = t.entries[(request.request_id + i) % MAX_PENDING_REQUESTS];
        if (entry.used)
            continue;
        entry = Entry{true, request.request_id, request.callback, request.error_callback,
                      request.callback_user_data, request.serialization};
        routed = request;
        routed.callback = onReply;
        routed.error_callback = onError;
        routed.callback_user_data = &entry;
        return true;
    }
    return false;
}

AAPXSSerializationContext* findRequestBuffer(uint32_t requestId) {
    auto& t = table();
    const std::lock_guard<NanoSleepLock> guard{t.lock};
    for (size_t i = 0; i < MAX_PENDING_REQUESTS; i++) {
        auto& entry = t.entries[(requestId + i) % MAX_PENDING_REQUESTS];
        if (entry.used && entry.request_id == requestId)
            return entry.buffer;
    }
    return nullptr;
}

void filterOutMessages(void* buffer) {
    auto mbh = (AAPMidiBufferHeader*) buffer;
    auto* data = (uint8_t*) (mbh + 1);
    uint32_t outputOffset = 0;
    uint32_t inputOffset = 0;
    uint8_t parseData[AAP_MIDI2_AAPXS_DATA_MAX_SIZE];
    uint8_t parseConversion[AAP_MIDI2_AAPXS_DATA_MAX_SIZE];
    aap_midi2_aapxs_parse_context parseContext{};
    aap_midi2_aapxs_parse_context_prepare(&parseContext, parseData, parseConversion, AAP_MIDI2_AAPXS_DATA_MAX_SIZE);

    while (inputOffset < mbh->length) {
        auto* iter = data + inputOffset;
        auto remaining = mbh->length - inputOffset;
        auto* ump = (cmidi2_ump*) iter;
        auto messageSize = cmidi2_ump_get_message_size_bytes(ump);
        if (messageSize <= 0)
            break;
        if (aap_midi2_parse_aapxs_sysex8(&parseContext, iter, remaining)) {
            // skip the whole message, up to and including its END packet
            while (true) {
                auto status = cmidi2_ump_get_status_code(ump);
                inputOffset += messageSize;
                if (status == CMIDI2_SYSEX_END || inputOffset >= mbh->length)
                    break;
                ump = (cmidi2_ump*) (data + inputOffset);
                if (cmidi2_ump_get_message_type(ump) != CMIDI2_MESSAGE_TYPE_SYSEX8_MDS)
                    break;
                messageSize = cmidi2_ump_get_message_size_bytes(ump);
            }
            continue;
        }

        if (outputOffset + static_cast<uint32_t>(messageSize) > mbh->length)
            break;
        if (outputOffset != inputOffset)
            memmove(data + outputOffset, iter, messageSize);
        outputOffset += messageSize;
        inputOffset += messageSize;
    }

    mbh->length = outputOffset;
}
}

}
