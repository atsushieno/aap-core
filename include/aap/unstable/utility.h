#ifndef AAP_CORE_UNSTABLE_UTILITY_H
#define AAP_CORE_UNSTABLE_UTILITY_H

#include <sys/time.h>
#include <assert.h>
#ifdef AAP_VERIFY_REALTIME
#include <cstdlib>
#include "aap/core/realtime.h"
#endif

#define AAP_ASSERT_FALSE assert(false)

namespace aap {

    // I don't think any simple and stupid SpinLock works well on mobiles, as we do not want to dry up battery.
    class NanoSleepLock {
        std::atomic_flag state = ATOMIC_FLAG_INIT;
    public:
        void lock() noexcept {
#ifdef AAP_VERIFY_REALTIME
            if (aap::RealtimeScope::isActive()) std::abort();
#endif
            const auto delay = timespec{0, 1000}; // 1 microsecond
            while(state.test_and_set())
                clock_nanosleep(CLOCK_REALTIME, 0, &delay, nullptr);
        }
        void unlock() noexcept { state.clear(); }
        bool try_lock() noexcept {
#ifdef AAP_VERIFY_REALTIME
            if (aap::RealtimeScope::isActive()) std::abort();
#endif
            return !state.test_and_set();
        }
    };

}


#endif//AAP_CORE_UNSTABLE_UTILITY_H
