#include <algorithm>
#include "aap/core/aapxs/typed-aapxs.h"

#include "../hosting/aapxs-transport.h"

namespace aap::xs {
    thread_local unsigned TypedAAPXS::blocking_depth = 0;

    void TypedAAPXS::cancelPendingTransportRequests(const std::string& error) {
        std::vector<std::pair<uint32_t, void*>> pending;
        {
            std::lock_guard<std::mutex> lock(calls_mutex);
            pending.reserve(in_flight.size());
            for (auto& entry : in_flight)
                pending.emplace_back(entry.first, entry.second.get());
        }
        for (auto& entry : pending)
            internal::sysex8::cancelRequest(entry.first, entry.second, error.c_str());
    }

    TypedAAPXS::~TypedAAPXS() {
        lifetime->store(nullptr);
        unregisterForAbort();
        detachAllPending("AAPXS owner destroyed");
    }

    void TypedAAPXS::registerForAbort() {
        if (!aapxs_instance || !aapxs_instance->register_abort_handler || !aapxs_instance->unregister_abort_handler) return;
        unregister_abort = aapxs_instance->unregister_abort_handler;
        abort_registration = aapxs_instance->register_abort_handler(aapxs_instance, this,
            [](void* client, const char* error) { static_cast<TypedAAPXS*>(client)->failAllPending(error); });
    }

    void TypedAAPXS::unregisterForAbort() {
        auto token = abort_registration;
        abort_registration = nullptr;
        if (token && unregister_abort) unregister_abort(token);
    }
}
