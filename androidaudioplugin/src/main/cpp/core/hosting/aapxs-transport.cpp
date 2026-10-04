#include "aapxs-transport.h"
#include "aapxs-midi2-session-internal.h"
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
    // Protected by the channel mutex. Completion/abort claim delivery only once.
    bool finished{false};
    bool advance_ready{false};
    bool delivery_done{false};
    std::shared_ptr<Pending> previous{};
    // Abort waits for a claimed completion to finish using the callback context.
    std::recursive_mutex delivery_mutex{};
    // set for requests without callback, whose sender waits for the result.
    std::shared_ptr<std::promise<std::string>> sync{};
};

namespace {
thread_local AAPXSBinderChannel* delivering_channel{};
struct BinderDeliveryScope {
    AAPXSBinderChannel* previous{delivering_channel};
    explicit BinderDeliveryScope(AAPXSBinderChannel* channel) { delivering_channel = channel; }
    ~BinderDeliveryScope() { delivering_channel = previous; }
};
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
    // A callback may issue a blocking request on this same channel. Release its head before
    // that nested send, retaining the active delivery in the private predecessor chain.
    if (delivering_channel == this) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (current && current->finished)
                current->advance_ready = true;
        }
        sendNext();
    }
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
        pending->previous = current;
        current = pending;
    }
    if (!dispatch(pending)) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (current != pending || pending->finished)
                return !pending->sync; // another path already delivered the result
            pending->finished = true;
            pending->delivery_done = true;
            pending->advance_ready = true;
        }
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
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (current != pending || pending->finished)
            return true; // aborted before dispatch; do not touch its released request buffer
        auto source = pending->request.serialization;
        size_t size = source ? source->data_size : 0;
        if (size > shared_block->data_capacity)
            return false;
        if (size > 0 && source != shared_block)
            memcpy(shared_block->data, source->data, size);
        shared_block->data_size = size;
    }

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
    std::lock_guard<std::recursive_mutex> deliveryLock(pending->delivery_mutex);
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (current.get() != pending || pending->finished)
            return; // aborted; the request buffer may be gone already
        pending->finished = true;
        auto target = pending->request.serialization;
        if (!error && target && target != shared_block) {
            auto reportedSize = reply_size ? reply_size() : std::nullopt;
            auto size = reportedSize.value_or(shared_block->data_capacity);
            if (size > shared_block->data_capacity || (reportedSize.has_value() && size > target->data_capacity)) {
                error = "AAPXS reply length exceeds buffer capacity";
                target->data_size = 0;
            } else {
                size = std::min(size, target->data_capacity);
                if (size) memcpy(target->data, shared_block->data, size);
                target->data_size = size;
            }
        }
    }
    {
        BinderDeliveryScope scope(this);
        if (pending->sync)
            pending->sync->set_value(error ? error : "");
        else
            deliver(pending->request, error, pluginOrHost);
    }
    // Keep this request visible to abort until its callback has returned. Only a head whose
    // delivery has finished may advance; stale completions cannot release a replacement head.
    {
        std::lock_guard<std::mutex> lock(mutex);
        pending->delivery_done = true;
        if (current.get() == pending)
            pending->advance_ready = true;
        // Remove finished predecessors, without losing any still-active outer callback.
        if (current) {
            auto link = &current->previous;
            while (*link) {
                if ((*link)->delivery_done)
                    *link = (*link)->previous;
                else
                    link = &(*link)->previous;
            }
        }
        if (current && current->delivery_done && !current->previous && !busy)
            current.reset();
    }
    sendNext();
}

void AAPXSBinderChannel::sendNext() {
    while (true) {
        std::shared_ptr<Pending> next;
        {
            std::lock_guard<std::mutex> lock(mutex);
            // An old completion may run after abort/replacement or another queue advance.
            if (!current || !current->advance_ready)
                return;
            auto link = &current->previous;
            while (*link) {
                if ((*link)->delivery_done)
                    *link = (*link)->previous;
                else
                    link = &(*link)->previous;
            }
            if (queue.empty()) {
                busy = false;
                if (current->delivery_done && !current->previous)
                    current.reset();
                idle.notify_all();
                return;
            }
            next = queue.front();
            queue.pop_front();
            next->previous = current;
            current = next;
        }
        if (dispatch(next))
            return;
        std::lock_guard<std::recursive_mutex> deliveryLock(next->delivery_mutex);
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (current != next || next->finished)
                return;
            next->finished = true;
        }
        {
            BinderDeliveryScope scope(this);
            deliver(next->request, "request could not be sent", nullptr);
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            next->delivery_done = true;
            if (current == next)
                next->advance_ready = true;
        }
    }
}

void AAPXSBinderChannel::abort(const char* error, void* pluginOrHost) {
    std::shared_ptr<Pending> inFlight;
    std::deque<std::shared_ptr<Pending>> dropped;
    bool deliverCurrent = false;
    std::vector<std::shared_ptr<Pending>> activeDeliveries;
    {
        std::lock_guard<std::mutex> lock(mutex);
        inFlight = std::move(current);
        current.reset();
        for (auto pending = inFlight; pending; pending = pending->previous)
            if (pending->finished && !pending->delivery_done)
                activeDeliveries.push_back(pending);
        if (inFlight && !inFlight->finished) {
            inFlight->finished = true;
            deliverCurrent = true;
        }
        dropped = std::move(queue);
        queue.clear();
        for (auto& pending : dropped)
            pending->finished = true;
        busy = false;
        idle.notify_all();
    }
    if (inFlight) {
        std::lock_guard<std::recursive_mutex> deliveryLock(inFlight->delivery_mutex);
        if (deliverCurrent) {
            if (inFlight->sync)
                inFlight->sync->set_value(error ? error : "error");
            else
                deliver(inFlight->request, error, pluginOrHost);
        }
    }
    for (auto& pending : activeDeliveries) {
        std::lock_guard<std::recursive_mutex> deliveryLock(pending->delivery_mutex);
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
                                                          const std::function<AAPXSBinderChannel::Transmit()>& createTransmit,
                                                          AAPXSBinderChannel::ReplySize replySize) {
    auto& r = registry();
    std::lock_guard<std::mutex> lock(r.mutex);
    auto& channel = r.channels[owner][sharedBlock];
    if (!channel)
        channel = std::make_shared<AAPXSBinderChannel>(sharedBlock, createTransmit(), std::move(replySize));
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
    bool delivering{false};
    uintptr_t token{0};
    uint32_t request_id{0};
    aapxs_completion_callback callback{nullptr};
    aapxs_error_callback error_callback{nullptr};
    void* callback_user_data{nullptr};
    AAPXSSerializationContext* buffer{nullptr};
    AAPXSMidi2InitiatorSession* session{nullptr};
};

constexpr size_t MAX_PENDING_REQUESTS = 1024;

struct Table {
    NanoSleepLock lock{};
    std::recursive_mutex unowned_gate{};
    uintptr_t next_token{0};
    std::array<Entry, MAX_PENDING_REQUESTS> entries{};
};

Table& table() {
    static auto* t = new Table();
    return *t;
}

Entry lookupToken(uintptr_t token) {
    const std::lock_guard<NanoSleepLock> guard{table().lock};
    for (auto& entry : table().entries)
        if (entry.used && entry.token == token)
            return entry;
    return {};
}

// Keep the slot reserved while delivering, so cancellation can find its session gate and wait
// until the reply copy and callback are finished. Tokens prevent an old callback taking a newly
// registered request that happens to reuse the same slot.
void deliverToken(uintptr_t token, void* pluginOrHost, const char* error) {
    auto snapshot = lookupToken(token);
    if (!snapshot.used)
        return;
    auto state = snapshot.session ? getAAPXSMidi2SessionState(snapshot.session) : nullptr;
    std::lock_guard<std::recursive_mutex> gate{state ? state->gate : table().unowned_gate};
    Entry completed{};
    {
        const std::lock_guard<NanoSleepLock> guard{table().lock};
        for (auto& entry : table().entries) {
            if (!entry.used || entry.token != token || entry.delivering)
                continue;
            entry.delivering = true;
            completed = entry;
            break;
        }
    }
    if (!completed.used)
        return;
    struct Release {
        uintptr_t token;
        ~Release() {
            const std::lock_guard<NanoSleepLock> guard{table().lock};
            for (auto& entry : table().entries)
                if (entry.used && entry.token == token) {
                    entry = Entry{};
                    break;
                }
        }
    } release{token};
    if (error && completed.error_callback)
        completed.error_callback(completed.callback_user_data, pluginOrHost, error);
    else if (completed.callback)
        completed.callback(completed.callback_user_data, pluginOrHost);
}

void onReply(void* context, void* pluginOrHost) {
    deliverToken(reinterpret_cast<uintptr_t>(context), pluginOrHost, nullptr);
}

void onError(void* context, void* pluginOrHost, const char* error) {
    deliverToken(reinterpret_cast<uintptr_t>(context), pluginOrHost, error ? error : "error");
}

bool cancel(uint32_t requestId, void* callbackContext, const char* error, void* pluginOrHost) {
    auto& t = table();
    Entry snapshot{};
    {
        const std::lock_guard<NanoSleepLock> guard{t.lock};
        for (auto& entry : t.entries)
            if (entry.used && entry.request_id == requestId && entry.callback_user_data == callbackContext) {
                snapshot = entry;
                break;
            }
    }
    if (!snapshot.used)
        return false;
    auto state = snapshot.session ? getAAPXSMidi2SessionState(snapshot.session) : nullptr;
    std::lock_guard<std::recursive_mutex> gate{state ? state->gate : t.unowned_gate};
    Entry cancelled{};
    {
        const std::lock_guard<NanoSleepLock> guard{t.lock};
        for (auto& entry : t.entries) {
            if (!entry.used || entry.token != snapshot.token)
                continue;
            if (entry.delivering)
                return true; // reentrant cancellation of the callback currently being delivered
            cancelled = entry;
            entry = Entry{};
            break;
        }
    }
    if (!cancelled.used)
        return false; // completed while waiting for the session gate
    if (cancelled.session)
        AAPXSMidi2SessionAccess::forgetRequest(*cancelled.session, requestId);
    if (cancelled.error_callback)
        cancelled.error_callback(cancelled.callback_user_data, pluginOrHost, error);
    else if (cancelled.callback)
        cancelled.callback(cancelled.callback_user_data, pluginOrHost);
    return true;
}
}

bool registerRequest(const AAPXSRequestContext& request, AAPXSRequestContext& routed) {
    return registerRequest(request, routed, nullptr);
}

bool registerRequest(const AAPXSRequestContext& request, AAPXSRequestContext& routed,
                     AAPXSMidi2InitiatorSession* session) {
    auto& t = table();
    const std::lock_guard<NanoSleepLock> guard{t.lock};
    for (auto& entry : t.entries)
        if (entry.used && entry.request_id == request.request_id)
            return false;
    for (size_t i = 0; i < MAX_PENDING_REQUESTS; i++) {
        auto& entry = t.entries[(request.request_id + i) % MAX_PENDING_REQUESTS];
        if (entry.used)
            continue;
        uintptr_t token;
        do {
            token = ++t.next_token;
        } while (token == 0 || std::any_of(t.entries.begin(), t.entries.end(), [token](auto& e) {
            return e.used && e.token == token;
        }));
        entry = Entry{true, false, token, request.request_id, request.callback, request.error_callback,
                      request.callback_user_data, request.serialization, session};
        routed = request;
        routed.callback = onReply;
        routed.error_callback = onError;
        routed.callback_user_data = reinterpret_cast<void*>(token);
        return true;
    }
    return false;
}

bool cancelRequest(uint32_t requestId, void* callbackContext, const char* error) {
    return cancel(requestId, callbackContext, error, nullptr);
}

void cancelRequestsForSession(AAPXSMidi2InitiatorSession* session, const char* error, void* pluginOrHost) {
    std::array<Entry, MAX_PENDING_REQUESTS> pending{};
    size_t count = 0;
    {
        const std::lock_guard<NanoSleepLock> guard{table().lock};
        for (auto& entry : table().entries)
            if (entry.used && entry.session == session && !entry.delivering)
                pending[count++] = entry;
    }
    for (size_t i = 0; i < count; i++)
        cancel(pending[i].request_id, pending[i].callback_user_data, error, pluginOrHost);
}

AAPXSSerializationContext* findRequestBuffer(uint32_t requestId) {
    auto& t = table();
    const std::lock_guard<NanoSleepLock> guard{t.lock};
    for (size_t i = 0; i < MAX_PENDING_REQUESTS; i++) {
        auto& entry = t.entries[(requestId + i) % MAX_PENDING_REQUESTS];
        if (entry.used && !entry.delivering && entry.request_id == requestId)
            return entry.buffer;
    }
    return nullptr;
}

void filterOutMessages(void* buffer) {
    filterOutMessages(buffer, nullptr, nullptr);
}

void filterOutMessages(void* buffer, void* sinkContext, void (*sink)(void*, const void*, size_t)) {
    if (!buffer) return;
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
            auto start = inputOffset;
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
            if (sink) sink(sinkContext, data + start, inputOffset - start);
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
