#ifndef AAP_CORE_REALTIME_H
#define AAP_CORE_REALTIME_H

namespace aap {
// Marks application processing paths. General typed AAPXS calls cannot be used here;
// payload-free host notifications use a separate preallocated handoff.
class __attribute__((visibility("default"))) RealtimeScope {
    int slot{-1};
public:
    RealtimeScope() noexcept;
    ~RealtimeScope();
    RealtimeScope(const RealtimeScope&) = delete;
    RealtimeScope& operator=(const RealtimeScope&) = delete;
    static bool isActive() noexcept;
};
}
#endif
