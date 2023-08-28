#ifndef AAP_CORE_UNSTABLE_UTILITY_H
#define AAP_CORE_UNSTABLE_UTILITY_H

#include <sys/time.h>

namespace aap {

    // I don't think any simple and stupid SpinLock works well on mobiles, as we do not want to dry up battery.
    class NanoSleepLock {
        std::atomic_flag state = ATOMIC_FLAG_INIT;
    public:
        void lock() noexcept {
            const auto delay = timespec{0, 1000}; // 1 microsecond
            while(state.test_and_set())
                clock_nanosleep(CLOCK_REALTIME, 0, &delay, nullptr);
        }
        void unlock() noexcept { state.clear(); }
        bool try_lock() noexcept { return !state.test_and_set(); }
    };

}


#endif//AAP_CORE_UNSTABLE_UTILITY_H