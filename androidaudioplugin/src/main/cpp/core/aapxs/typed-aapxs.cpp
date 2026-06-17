#include "aap/core/aapxs/typed-aapxs.h"
#include "aap/core/host/plugin-instance.h"

// These are defined out-of-line (not in typed-aapxs.h) because they need the full
// aap::PluginInstance definition, and typed-aapxs.h is included *by* plugin-instance.h
// (via standard-extensions.h) — including it back here would be circular.

namespace aap::xs {

    TypedAAPXS::~TypedAAPXS() {
        unregisterForAbort();
    }

    void TypedAAPXS::registerForAbort() {
        if (!aapxs_instance || !aapxs_instance->host_context)
            return;
        // host_context is the owning PluginInstance (set in AAPXS*Dispatcher::setupInstances).
        ((PluginInstance*) aapxs_instance->host_context)->registerAsyncAbortable(this);
    }

    void TypedAAPXS::unregisterForAbort() {
        if (!aapxs_instance || !aapxs_instance->host_context)
            return;
        ((PluginInstance*) aapxs_instance->host_context)->unregisterAsyncAbortable(this);
    }
}
