#ifndef AAP_CORE_AAPXS_RESULT_H
#define AAP_CORE_AAPXS_RESULT_H

#include <string>

namespace aap {
    /**
     * A simple value-or-error result type used across the (non-realtime) AAPXS API surface.
     *
     * `error` is empty on success and carries a human-readable description on failure
     * (e.g. "timeout", "service disconnected"). We deliberately use `std::string` instead of
     * throwing exceptions: AAPXS calls may run across a Binder transaction or on the audio
     * thread, where an uncaught C++ exception would abort `AudioPluginService`.
     *
     * Note: this type is heap/allocation-bearing (std::string) and therefore is only for the
     * non-realtime AAPXS functions. RT-safe-ish functions must not use it.
     */
    template<typename T>
    struct Result {
        T value;
        std::string error;

        bool isOk() const { return error.empty(); }
    };
}

#endif //AAP_CORE_AAPXS_RESULT_H
