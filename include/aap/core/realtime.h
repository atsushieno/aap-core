#ifndef AAP_CORE_REALTIME_H
#define AAP_CORE_REALTIME_H

namespace aap {
// Marks application processing paths. General typed AAPXS calls cannot be used here;
// payload-free host notifications use a separate preallocated handoff.
class RealtimeScope {
    inline static thread_local unsigned depth{0};
public:
    RealtimeScope() noexcept { ++depth; }
    ~RealtimeScope() { --depth; }
    static bool isActive() noexcept { return depth != 0; }
};
}
#endif
