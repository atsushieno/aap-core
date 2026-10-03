#ifndef AAP_CORE_EVENT_WAKEUP_H
#define AAP_CORE_EVENT_WAKEUP_H

#include <atomic>
#include <chrono>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <system_error>
#include <unistd.h>
#ifdef __linux__
#include <sys/eventfd.h>
#include <poll.h>
#else
#include <fcntl.h>
#include <poll.h>
#endif

namespace aap::internal {
// One waiting consumer. Construct/destroy off processing, after producers stop.
// Linux/Android uses eventfd; the native macOS harness uses a nonblocking pipe.
// Publication precedes notify(). consume() precedes draining/rechecking work,
// so a producer racing with sleep either has its work observed or leaves an event.
class EventWakeup {
    static_assert(std::atomic<bool>::is_always_lock_free);
    int read_fd{-1}, write_fd{-1};
    std::atomic<bool> pending{false};
public:
    using Clock = std::chrono::steady_clock;
    using Deadline = Clock::time_point;
    EventWakeup() {
#ifdef __linux__
        read_fd = write_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (read_fd < 0) throw std::system_error(errno, std::generic_category(), "AAP eventfd");
#else
        int descriptors[2];
        if (pipe(descriptors) < 0) throw std::system_error(errno, std::generic_category(), "AAP wakeup pipe");
        read_fd = descriptors[0]; write_fd = descriptors[1];
        if (fcntl(read_fd, F_SETFL, O_NONBLOCK) < 0 || fcntl(write_fd, F_SETFL, O_NONBLOCK) < 0 ||
            fcntl(read_fd, F_SETFD, FD_CLOEXEC) < 0 || fcntl(write_fd, F_SETFD, FD_CLOEXEC) < 0) {
            auto error = errno;
            close(read_fd); close(write_fd);
            throw std::system_error(error, std::generic_category(), "AAP wakeup pipe flags");
        }
#endif
    }
    ~EventWakeup() {
        close(read_fd);
        if (write_fd != read_fd) close(write_fd);
    }
    EventWakeup(const EventWakeup&) = delete;
    EventWakeup& operator=(const EventWakeup&) = delete;

    void notify() noexcept {
        if (pending.exchange(true, std::memory_order_acq_rel)) return;
        const uint64_t value = 1;
        ssize_t result;
        do { result = write(write_fd, &value, sizeof(value)); } while (result < 0 && errno == EINTR);
        // EAGAIN means the descriptor already contains a wakeup. Other errors
        // violate the descriptor-lifetime invariant; there is no inline fallback.
    }

    void consume() noexcept {
        uint64_t value;
        ssize_t result;
        do { result = read(read_fd, &value, sizeof(value)); } while (result < 0 && errno == EINTR);
#ifndef __linux__
        // A pipe read consumes one token, unlike eventfd's counter read.
        while (result > 0) {
            do { result = read(read_fd, &value, sizeof(value)); } while (result < 0 && errno == EINTR);
        }
#endif
        pending.exchange(false, std::memory_order_acq_rel);
    }

    // Only the consumer blocks. No fixed-period wakeups; recompute after signals.
    void wait(Deadline deadline = Deadline::max()) noexcept {
        pollfd descriptor{read_fd, POLLIN, 0};
        for (;;) {
#ifdef __linux__
            timespec timeout{};
            timespec* limit = nullptr;
            if (deadline != Deadline::max()) {
                auto remaining = deadline - Clock::now();
                if (remaining <= Clock::duration::zero()) return;
                auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count();
                timeout = timespec{static_cast<time_t>(ns / 1000000000), static_cast<long>(ns % 1000000000)};
                limit = &timeout;
            }
            auto result = ppoll(&descriptor, 1, limit, nullptr);
#else
            int timeout = -1;
            if (deadline != Deadline::max()) {
                auto remaining = deadline - Clock::now();
                if (remaining <= Clock::duration::zero()) return;
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
                if (ms < remaining) ++ms;
                timeout = ms.count() > INT_MAX ? INT_MAX : static_cast<int>(ms.count());
            }
            auto result = poll(&descriptor, 1, timeout);
#endif
            if (result < 0 && errno == EINTR) continue;
            return;
        }
    }
};
}
#endif
