#ifndef AAP_CORE_HOSTING_AAPXS_TRANSPORT_H
#define AAP_CORE_HOSTING_AAPXS_TRANSPORT_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <mutex>
#include "aap/aapxs.h"

namespace aap { class AAPXSMidi2InitiatorSession; }

namespace aap::internal {

// Carries AAPXS requests over one extension's shared memory block, one at a time, in FIFO order.
// Each request brings its own buffer (`request->serialization`): the payload is copied into the
// block when the request is sent, and the reply is copied back before its callback runs, so that
// the callback may issue further requests. Not RT-safe; SysEx8 requests do not use it.
class AAPXSBinderChannel : public std::enable_shared_from_this<AAPXSBinderChannel> {
public:
    // Sends `request`, whose payload is already in the shared block. Returns false if it could not be sent.
    using Transmit = std::function<bool(const AAPXSRequestContext& request)>;

    using ReplySize = std::function<std::optional<size_t>()>;
    AAPXSBinderChannel(AAPXSSerializationContext* sharedBlock, Transmit transmit, ReplySize replySize = {})
            : shared_block(sharedBlock), transmit(std::move(transmit)), reply_size(std::move(replySize)) {}

    // The contract of send_aapxs_request(): returns true if the result arrives via request->callback.
    // A request without callback is completed synchronously.
    bool send(AAPXSRequestContext* request);

    // Fails queued requests and abandons the in-flight one, whose completion is then ignored.
    void abort(const char* error, void* pluginOrHost);

private:
    struct Pending;

    AAPXSSerializationContext* shared_block;
    Transmit transmit;
    ReplySize reply_size;
    std::mutex mutex{};
    std::condition_variable idle{};
    bool busy{false};
    std::shared_ptr<Pending> current{};
    std::deque<std::shared_ptr<Pending>> queue{};

    bool dispatch(const std::shared_ptr<Pending>& pending);
    void complete(Pending* pending, const char* error, void* pluginOrHost);
    // Hands the block to the next queued request, or marks it idle.
    void sendNext();
    static void onCompleted(void* context, void* pluginOrHost);
    static void onFailed(void* context, void* pluginOrHost, const char* error);
};

// Channels are per instance (`owner`) and per shared block; they are created on first use.
std::shared_ptr<AAPXSBinderChannel> getAAPXSBinderChannel(const void* owner,
                                                          AAPXSSerializationContext* sharedBlock,
                                                          const std::function<AAPXSBinderChannel::Transmit()>& createTransmit,
                                                          AAPXSBinderChannel::ReplySize replySize = {});
void abortAAPXSBinderChannels(const void* owner, const char* error, void* pluginOrHost);
void releaseAAPXSBinderChannels(const void* owner);

// Pending SysEx8 requests, so that each reply is written into its own request buffer.
// Bookkeeping is bounded; callback delivery and cancellation share the session gate.
namespace sysex8 {
    // Returns a copy of `request` whose callbacks go through the pending table, or false if the table is full.
    bool registerRequest(const AAPXSRequestContext& request, AAPXSRequestContext& routed);
    // Internal overload associates the request with the gate protecting its session.
    bool registerRequest(const AAPXSRequestContext& request, AAPXSRequestContext& routed,
                         AAPXSMidi2InitiatorSession* session);
    // Cancels a typed request before its callback context can be released.
    bool cancelRequest(uint32_t requestId, void* callbackContext, const char* error);
    void cancelRequestsForSession(AAPXSMidi2InitiatorSession* session, const char* error, void* pluginOrHost);
    // The buffer of the pending request `requestId`, or null if it is unknown (e.g. timed out).
    AAPXSSerializationContext* findRequestBuffer(uint32_t requestId);
    // Removes AAPXS SysEx8 messages from a MIDI2 port buffer (AAPMidiBufferHeader + UMPs), once
    // they are consumed, so that the plugin or the host does not see them as MIDI (e.g. MIDI thru
    // would otherwise echo the host's requests back as if they were replies).
    void filterOutMessages(void* midi2Buffer);
    // Copies each complete AAPXS wire message to a preallocated handoff before filtering.
    // The sink must not allocate, lock, or invoke extension/user callbacks.
    void filterOutMessages(void* midi2Buffer, void* sinkContext,
                           void (*sink)(void*, const void*, size_t));
}

}

#endif //AAP_CORE_HOSTING_AAPXS_TRANSPORT_H
