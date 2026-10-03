#include "aap/core/realtime.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <pthread.h>
#include <type_traits>

namespace {
// Shared across SDK/plugin DSOs, without cold thread-local initialization. A
// bounded scan also avoids allocating a registry entry on a processing thread.
constexpr unsigned capacity = 256;
struct alignas(64) ThreadScope {
    std::atomic<uintptr_t> key{0};
    std::atomic<unsigned> depth{0};
};
static_assert(std::atomic<uintptr_t>::is_always_lock_free);
static_assert(std::atomic<unsigned>::is_always_lock_free);
std::array<ThreadScope, capacity> scopes{};
std::atomic<unsigned> overflow{0};

template<class Identity> uintptr_t threadKey(Identity identity) noexcept {
    if constexpr (std::is_pointer_v<Identity>)
        return reinterpret_cast<uintptr_t>(identity);
    else
        return static_cast<uintptr_t>(identity);
}
unsigned bucket(uintptr_t key) noexcept { return ((key >> 8) ^ key) & (capacity - 1); }
}
#ifdef AAP_VERIFY_REALTIME
extern "C" void aap_test_enter_processing();
extern "C" void aap_test_leave_processing();
#endif

aap::RealtimeScope::RealtimeScope() noexcept {
    const auto key = threadKey(pthread_self());
    const auto start = bucket(key);
    for (unsigned i = 0; i < capacity; ++i) {
        const auto candidate = (start + i) & (capacity - 1);
        if (scopes[candidate].key.load(std::memory_order_relaxed) == key) {
            slot = static_cast<int>(candidate);
            break;
        }
    }
    if (slot < 0) {
        for (unsigned i = 0; i < capacity; ++i) {
            const auto candidate = (start + i) & (capacity - 1);
            uintptr_t empty = 0;
            if (scopes[candidate].key.compare_exchange_strong(empty, key,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                slot = static_cast<int>(candidate);
                break;
            }
        }
    }
    if (slot < 0)
        overflow.fetch_add(1, std::memory_order_relaxed);
    else
        scopes[slot].depth.fetch_add(1, std::memory_order_relaxed);
#ifdef AAP_VERIFY_REALTIME
    if (slot < 0 || scopes[slot].depth.load(std::memory_order_relaxed) == 1)
        aap_test_enter_processing();
#endif
}

aap::RealtimeScope::~RealtimeScope() {
    if (slot < 0) {
#ifdef AAP_VERIFY_REALTIME
        aap_test_leave_processing();
#endif
        overflow.fetch_sub(1, std::memory_order_relaxed);
    } else if (scopes[slot].depth.fetch_sub(1, std::memory_order_relaxed) == 1) {
#ifdef AAP_VERIFY_REALTIME
        aap_test_leave_processing();
#endif
        scopes[slot].key.store(0, std::memory_order_release);
    }
}

bool aap::RealtimeScope::isActive() noexcept {
    // Fail closed when more than 256 distinct threads are annotated at once:
    // refuse general SDK queries even on other callers until capacity returns.
    if (overflow.load(std::memory_order_relaxed)) return true;
    const auto key = threadKey(pthread_self());
    const auto start = bucket(key);
    for (unsigned i = 0; i < capacity; ++i) {
        const auto candidate = (start + i) & (capacity - 1);
        if (scopes[candidate].key.load(std::memory_order_relaxed) == key)
            return scopes[candidate].depth.load(std::memory_order_relaxed) != 0;
    }
    return false;
}
