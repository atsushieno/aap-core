#ifndef AAP_CORE_RECIPIENT_AAPXS_H
#define AAP_CORE_RECIPIENT_AAPXS_H

#include <memory>
#include "aap/aapxs.h"

namespace aap::xs {
// Retain this handle before an incoming handler returns to complete on another
// non-processing thread. It owns the request, URI and serialization storage;
// completing never requires retaining the plugin or recipient instance pointer.
class AAP_PUBLIC_API DeferredAAPXSReply {
public:
    virtual ~DeferredAAPXSReply() = default;
    virtual AAPXSRequestContext& request() = 0;
    // Exactly one successful publication. False means cancelled/already replied,
    // an invalid reply, or backpressure. Backpressure may be retried with this handle.
    virtual bool complete() = 0;
};

// Supported for owned incoming SysEx8 requests. Returns null for a borrowed
// Binder request or a processing-thread caller. The handler's original pointer
// is borrowed: use this handle's request() after returning from the handler.
// A retained handle remains safe to inspect/complete after instance teardown,
// but it does not keep plugin objects alive. Coordinate application-owned DSP
// or plugin state access separately. Release all handles when finished.
AAP_PUBLIC_API std::shared_ptr<DeferredAAPXSReply> retainAAPXSReply(AAPXSRequestContext* request);
}
#endif
