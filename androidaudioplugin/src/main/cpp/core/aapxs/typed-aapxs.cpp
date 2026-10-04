#include <algorithm>
#include "aap/core/aapxs/typed-aapxs.h"
#include "aap/core/host/plugin-instance.h"
#include "../hosting/aapxs-transport.h"

// These are defined out-of-line (not in typed-aapxs.h) because they need the full
// aap::PluginInstance definition, and typed-aapxs.h is included *by* plugin-instance.h
// (via standard-extensions.h) — including it back here would be circular.

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
        detachAllPending("AAPXS owner destroyed");
        unregisterForAbort();
    }

    void TypedAAPXS::registerForAbort() {
        if (!aapxs_instance || !aapxs_instance->host_context)
            return;
        // host_context is the owning PluginInstance (set in AAPXS*Dispatcher::setupInstances).
        // We take a shared_ptr to its abort registry and keep it for our whole lifetime, so
        // unregistering at destruction never touches a destroyed instance member.
        abort_registry = ((PluginInstance*) aapxs_instance->host_context)->getAsyncAbortRegistry();
        if (!abort_registry)
            return;
        std::lock_guard<std::mutex> lock(abort_registry->mutex);
        abort_registry->abortables.emplace_back(this);
    }

    void TypedAAPXS::unregisterForAbort() {
        if (!abort_registry)
            return;
        std::lock_guard<std::mutex> lock(abort_registry->mutex);
        auto& v = abort_registry->abortables;
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }
}
