

#include "aap/core/AAPXSMidi2InitiatorSession.h"
#include <cstdlib>
#include "aapxs-midi2-session-internal.h"
#include "aapxs-transport.h"
#include "aap/core/aap_midi2_helper.h"
#include "aap/ext/midi.h"
#include "aap/unstable/utility.h"
#include "aap/unstable/logging.h"
#include "../include_cmidi2.h"

#define LOG_TAG "AAP.XS"

namespace {
// This short registry lock protects sidecar lookup only. Each session's gate protects its
// encoding/parsing buffers and pending callbacks, including calls into addMidi2Event().
struct SessionStates {
    aap::NanoSleepLock lock{};
    std::map<const aap::AAPXSMidi2InitiatorSession*, std::shared_ptr<aap::internal::AAPXSMidi2SessionState>> items{};
};
SessionStates& sessionStates() {
    static auto* states = new SessionStates(); // sessions can be destroyed during process teardown
    return *states;
}

void reject(const AAPXSRequestContext& request, const char* error, void* pluginOrHost = nullptr) {
    if (request.error_callback)
        request.error_callback(request.callback_user_data, pluginOrHost, error);
    else if (request.callback)
        request.callback(request.callback_user_data, pluginOrHost);
}
}

aap::AAPXSMidi2InitiatorSession::AAPXSMidi2InitiatorSession(int32_t midiBufferSize)
        : midi_buffer_size(midiBufferSize) {
    // Requests are encoded at the beginning of these buffers, possibly on other threads, while
    // replies are parsed on the extension worker; parsing uses the extra space at the end.
    aapxs_rt_midi_buffer = (uint8_t*) calloc(1, midi_buffer_size + AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    aapxs_rt_conversion_helper_buffer = (uint8_t*) calloc(1, midi_buffer_size + AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    aap_midi2_aapxs_parse_context_prepare(&aapxs_parse_context,
                                          aapxs_rt_midi_buffer + midi_buffer_size,
                                          aapxs_rt_conversion_helper_buffer + midi_buffer_size,
                                          AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
    memset(pending_callbacks, 0, sizeof(CallbackUnit) * MAX_PENDING_CALLBACKS);
    const std::lock_guard<NanoSleepLock> guard{sessionStates().lock};
    sessionStates().items[this] = std::make_shared<internal::AAPXSMidi2SessionState>();
}

aap::AAPXSMidi2InitiatorSession::~AAPXSMidi2InitiatorSession() {
    auto state = internal::getAAPXSMidi2SessionState(this);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    internal::AAPXSMidi2SessionAccess::cancelPending(*this, "AAPXS session destroyed", nullptr);
    {
        const std::lock_guard<NanoSleepLock> guard{sessionStates().lock};
        sessionStates().items.erase(this);
    }
    if (aapxs_rt_midi_buffer)
        free(aapxs_rt_midi_buffer);
    if (aapxs_rt_conversion_helper_buffer)
        free(aapxs_rt_conversion_helper_buffer);
}

void aap::AAPXSMidi2InitiatorSession::addSession(
        add_midi2_event_func addMidi2Event,
        void* addMidi2EventUserData,
        int32_t group,
        int32_t requestId,
        uint8_t urid,
        const char* uri,
        void* data,
        int32_t dataSize,
        int32_t opcode) {
    auto state = internal::getAAPXSMidi2SessionState(this);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    if (state->closed)
        return;
    size_t size = aap_midi2_generate_aapxs_sysex8((uint32_t*) aapxs_rt_midi_buffer,
                                                  midi_buffer_size / sizeof(int32_t),
                                                  (uint8_t*) aapxs_rt_conversion_helper_buffer,
                                                  midi_buffer_size,
                                                  group,
                                                  requestId,
                                                  urid,
                                                  uri,
                                                  opcode,
                                                  (uint8_t*) data,
                                                  dataSize);
    addMidi2Event(this, addMidi2EventUserData, size);
}

void aap::AAPXSMidi2InitiatorSession::addSession(add_midi2_event_func addMidi2Event,
                                                 void* addMidi2EventUserData,
                                                 AAPXSRequestContext *request) {
    auto state = internal::getAAPXSMidi2SessionState(this);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    if (state->closed) {
        reject(*request, "AAPXS session closed");
        return;
    }
    const char* error = nullptr;
    bool deadlineChanged = false;
    {
        size_t slot = MAX_PENDING_CALLBACKS;
        if (request->callback) {
            for (size_t i = 0; i < MAX_PENDING_CALLBACKS; i++)
                if (!pending_callbacks[i].func) {
                    slot = i;
                    break;
                }
            if (slot == MAX_PENDING_CALLBACKS)
                error = "too many pending AAPXS callbacks";
        }
        if (!error) {
            size_t size = aap_midi2_generate_aapxs_sysex8(
                    (uint32_t*) aapxs_rt_midi_buffer, midi_buffer_size / sizeof(int32_t),
                    aapxs_rt_conversion_helper_buffer, midi_buffer_size,
                    0, request->request_id, request->urid, request->uri, request->opcode,
                    (uint8_t*) request->serialization->data, request->serialization->data_size);
            if (size == 0)
                error = "AAPXS request could not be encoded";
            else {
                if (request->callback) {
                    deadlineChanged = true;
                    pending_callbacks[slot] = CallbackUnit{request->request_id, request->callback,
                            request->callback_user_data, request->error_callback,
                            std::chrono::steady_clock::now() + std::chrono::milliseconds(request_timeout_ms)};
                }
                if (!addMidi2Event(this, addMidi2EventUserData, size)) {
                    if (request->callback) pending_callbacks[slot] = {};
                    error = "AAPXS MIDI handoff full";
                }
            }
        }
    }
    if (deadlineChanged && state->deadline_changed) state->deadline_changed();
    // This may complete and delete the caller's request context. Do not access it afterwards.
    if (error)
        reject(*request, error);
}

void aap::AAPXSMidi2InitiatorSession::sweepTimeouts(void* pluginOrHost) {
    auto now = std::chrono::steady_clock::now();
    CallbackUnit expired[MAX_PENDING_CALLBACKS];
    size_t numExpired = 0;
    {
        for (size_t i = 0; i < MAX_PENDING_CALLBACKS; i++) {
            auto& unit = pending_callbacks[i];
            if (!unit.func)
                continue;
            if (now < unit.deadline)
                continue;
            expired[numExpired++] = unit;
            memset(&unit, 0, sizeof(CallbackUnit));
        }
    }
    for (size_t i = 0; i < numExpired; i++)
        if (expired[i].error_func)
            expired[i].error_func(expired[i].data, pluginOrHost, "timeout");
        else if (expired[i].func)
            expired[i].func(expired[i].data, pluginOrHost);
}

void aap::AAPXSMidi2InitiatorSession::completeSession(void* buffer, void* pluginOrHost) {
    auto state = internal::getAAPXSMidi2SessionState(this);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    if (state->closed)
        return;
    auto mbh = (AAPMidiBufferHeader *) buffer;
    void* data = mbh + 1;
    CMIDI2_UMP_SEQUENCE_FOREACH(data, mbh->length, iter) {
        auto umpSize = mbh->length - ((uint8_t*) iter - (uint8_t*) data);
        if (aap_midi2_parse_aapxs_sysex8(&aapxs_parse_context, iter, umpSize)) {
            // Claim the callback before touching its request buffer. Cancelled/unsolicited plugin
            // replies have no slot and must not reach the extension reply handler.
            CallbackUnit unit{};
            {
                for (size_t i = 0; i < MAX_PENDING_CALLBACKS; i++) {
                    if (pending_callbacks[i].func && pending_callbacks[i].request_id == aapxs_parse_context.request_id) {
                        unit = pending_callbacks[i];
                        memset(pending_callbacks + i, 0, sizeof(CallbackUnit));
                        break;
                    }
                }
            }
            if (unit.func) {
                if (handle_reply)
                    handle_reply(&aapxs_parse_context);
                unit.func(unit.data, pluginOrHost);
            } else if (aapxs_parse_context.opcode < 0 && handle_reply) {
                // Older plugin transports may send unsolicited host requests over SysEx8.
                handle_reply(&aapxs_parse_context);
            }
        }

        // FIXME: should we remove those AAPXS SysEx8 from the UMP buffer?
        //  It is going to be extraneous to the host.
    }

    // Detect and fail any requests whose reply never arrived within the timeout window.
    sweepTimeouts(pluginOrHost);
}

namespace aap::internal {
std::shared_ptr<AAPXSMidi2SessionState> getAAPXSMidi2SessionState(const AAPXSMidi2InitiatorSession* session) {
    const std::lock_guard<NanoSleepLock> guard{sessionStates().lock};
    auto it = sessionStates().items.find(session);
    return it == sessionStates().items.end() ? nullptr : it->second;
}

void AAPXSMidi2SessionAccess::setDeadlineChangedHandler(AAPXSMidi2InitiatorSession& session, std::function<void()> handler) {
    auto state = getAAPXSMidi2SessionState(&session);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    state->deadline_changed = std::move(handler);
}

std::chrono::steady_clock::time_point AAPXSMidi2SessionAccess::nextDeadline(const AAPXSMidi2InitiatorSession& session) {
    auto state = getAAPXSMidi2SessionState(&session);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    auto deadline = std::chrono::steady_clock::time_point::max();
    if (!state->closed)
        for (auto& unit : session.pending_callbacks)
            if (unit.func && unit.deadline < deadline) deadline = unit.deadline;
    return deadline;
}

bool AAPXSMidi2SessionAccess::sendRequest(AAPXSMidi2InitiatorSession& session,
                                        add_midi2_event_func addEvent, void* userData,
                                        AAPXSRequestContext* request) {
    auto state = getAAPXSMidi2SessionState(&session);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    if (state->closed) {
        bool completes = request->callback != nullptr;
        reject(*request, "AAPXS session closed");
        return completes;
    }
    AAPXSRequestContext routed = *request;
    if (request->callback && !sysex8::registerRequest(*request, routed, &session)) {
        if (request->error_callback) {
            reject(*request, "too many pending AAPXS requests");
            return true;
        }
        return false;
    }
    session.addSession(addEvent, userData, &routed);
    return true;
}

void AAPXSMidi2SessionAccess::forgetRequest(AAPXSMidi2InitiatorSession& session, uint32_t requestId) {
    auto state = getAAPXSMidi2SessionState(&session);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    for (auto& unit : session.pending_callbacks)
        if (unit.func && unit.request_id == requestId) {
            unit = {};
            if (state->deadline_changed) state->deadline_changed();
            return;
        }
}

void AAPXSMidi2SessionAccess::cancelPending(AAPXSMidi2InitiatorSession& session,
                                         const char* error, void* pluginOrHost) {
    auto state = getAAPXSMidi2SessionState(&session);
    const std::lock_guard<std::recursive_mutex> delivery{state->gate};
    state->closed = true;
    AAPXSMidi2InitiatorSession::CallbackUnit pending[MAX_PENDING_CALLBACKS];
    {
        std::copy(std::begin(session.pending_callbacks), std::end(session.pending_callbacks), pending);
        std::fill(std::begin(session.pending_callbacks), std::end(session.pending_callbacks),
                  AAPXSMidi2InitiatorSession::CallbackUnit{});
    }
    if (state->deadline_changed) state->deadline_changed();
    for (auto& unit : pending) {
        if (unit.error_func)
            unit.error_func(unit.data, pluginOrHost, error);
        else if (unit.func)
            unit.func(unit.data, pluginOrHost);
    }
    sysex8::cancelRequestsForSession(&session, error, pluginOrHost);
}
}
