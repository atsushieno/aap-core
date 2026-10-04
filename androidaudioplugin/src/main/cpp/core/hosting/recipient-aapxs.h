#ifndef AAP_CORE_HOSTING_RECIPIENT_AAPXS_H
#define AAP_CORE_HOSTING_RECIPIENT_AAPXS_H

#include <functional>
#include <memory>
#include "aap/core/aapxs/recipient-aapxs.h"

namespace aap::internal {
// Only worker/control threads touch this store. DSP sees the existing byte queue.
class RecipientRequestStore {
    struct State;
    std::shared_ptr<State> state;
public:
    using Sender = std::function<bool(const AAPXSRequestContext&)>;
    RecipientRequestStore();
    ~RecipientRequestStore();
    void setSender(Sender sender);
    std::shared_ptr<xs::DeferredAAPXSReply> create(const AAPXSRequestContext& request,
                                                 size_t capacity);
    // Waits for any publishing sender and detaches it before its target is freed.
    void close();
};
}
#endif
