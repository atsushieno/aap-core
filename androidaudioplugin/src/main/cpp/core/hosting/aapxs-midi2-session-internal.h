#ifndef AAP_CORE_AAPXS_MIDI2_SESSION_INTERNAL_H
#define AAP_CORE_AAPXS_MIDI2_SESSION_INTERNAL_H

#include <memory>
#include <mutex>
#include "aap/core/AAPXSMidi2InitiatorSession.h"

namespace aap::internal {
// Sidecar storage preserves the public session's size and layout. The gate spans reply copying
// and callback delivery, so cancellation cannot free a buffer between those operations.
struct AAPXSMidi2SessionState {
    std::recursive_mutex gate;
    bool closed{false};
    std::function<void()> deadline_changed;
};
std::shared_ptr<AAPXSMidi2SessionState> getAAPXSMidi2SessionState(const AAPXSMidi2InitiatorSession* session);

struct AAPXSMidi2SessionAccess {
    static void setDeadlineChangedHandler(AAPXSMidi2InitiatorSession& session, std::function<void()> handler);
    static std::chrono::steady_clock::time_point nextDeadline(const AAPXSMidi2InitiatorSession& session);
    static bool sendRequest(AAPXSMidi2InitiatorSession& session, add_midi2_event_func addEvent,
                            void* userData, AAPXSRequestContext* request);
    static void cancelPending(AAPXSMidi2InitiatorSession& session, const char* error, void* pluginOrHost);
    static void forgetRequest(AAPXSMidi2InitiatorSession& session, uint32_t requestId);
};
}
#endif
