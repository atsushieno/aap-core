#include "host-aapxs-request-queue.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <pthread.h>
#include <semaphore.h>

namespace {

constexpr size_t QUEUE_CAPACITY = 256;
constexpr size_t MAX_CLOSED_OWNERS = 64;

struct QueueState {
    aap::NanoSleepLock lock{};
    std::array<aap::internal::HostAAPXSRequest, QUEUE_CAPACITY> ring{};
    size_t head{0};
    size_t count{0};
    std::array<const void*, MAX_CLOSED_OWNERS> closed_owners{};
    const void* sending_owner{nullptr};
    sem_t available{};
    std::once_flag started{};

    bool isClosed(const void* owner) const {
        return std::find(closed_owners.begin(), closed_owners.end(), owner) != closed_owners.end();
    }

    bool pop(aap::internal::HostAAPXSRequest& request) {
        const std::lock_guard<aap::NanoSleepLock> guard{lock};
        if (count == 0)
            return false;
        request = ring[head];
        head = (head + 1) % QUEUE_CAPACITY;
        count--;
        sending_owner = request.owner;
        return true;
    }

    void run() {
        pthread_setname_np(pthread_self(), "AAP.HostAAPXS");
        while (true) {
            while (sem_wait(&available) != 0 && errno == EINTR) {}
            aap::internal::HostAAPXSRequest request{};
            while (pop(request)) {
                request.sendNow();
                const std::lock_guard<aap::NanoSleepLock> guard{lock};
                sending_owner = nullptr;
            }
        }
    }
};

// Intentionally never destroyed: the worker runs for the process lifetime.
QueueState& state() {
    static auto* s = new QueueState();
    return *s;
}

}

aap::internal::HostAAPXSRequestQueue& aap::internal::HostAAPXSRequestQueue::getInstance() {
    static HostAAPXSRequestQueue instance;
    return instance;
}

void aap::internal::HostAAPXSRequestQueue::start() {
    auto& s = state();
    std::call_once(s.started, [&s] {
        sem_init(&s.available, 0, 0);
        std::thread([&s] { s.run(); }).detach();
    });
}

aap::internal::HostAAPXSRequestQueue::EnqueueResult
aap::internal::HostAAPXSRequestQueue::enqueue(const HostAAPXSRequest& request) {
    auto& s = state();
    {
        const std::lock_guard<NanoSleepLock> guard{s.lock};
        if (s.isClosed(request.owner))
            return EnqueueResult::OwnerClosed;
        if (s.count == QUEUE_CAPACITY)
            return EnqueueResult::Full;
        s.ring[(s.head + s.count) % QUEUE_CAPACITY] = request;
        s.count++;
    }
    sem_post(&s.available);
    return EnqueueResult::Queued;
}

void aap::internal::HostAAPXSRequestQueue::closeOwner(const void* owner) {
    auto& s = state();
    while (true) {
        {
            const std::lock_guard<NanoSleepLock> guard{s.lock};
            size_t kept = 0;
            for (size_t i = 0; i < s.count; i++) {
                auto& r = s.ring[(s.head + i) % QUEUE_CAPACITY];
                if (r.owner != owner)
                    s.ring[(s.head + kept++) % QUEUE_CAPACITY] = r;
            }
            s.count = kept;
            if (!s.isClosed(owner)) {
                auto slot = std::find(s.closed_owners.begin(), s.closed_owners.end(), nullptr);
                if (slot != s.closed_owners.end())
                    *slot = owner;
            }
            if (s.sending_owner != owner)
                return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void aap::internal::HostAAPXSRequestQueue::forgetOwner(const void* owner) {
    auto& s = state();
    const std::lock_guard<NanoSleepLock> guard{s.lock};
    std::replace(s.closed_owners.begin(), s.closed_owners.end(), owner, (const void*) nullptr);
}
