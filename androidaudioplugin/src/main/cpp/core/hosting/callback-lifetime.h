#pragma once
#include <functional>
#include <mutex>
#include <utility>

namespace aap::internal {
// A callback borrows its owner while holding the gate. Retirement blocks other threads and
// defers reentrant destruction until the outermost callback returns. Storage is private.
template<typename T>
class __attribute__((visibility("hidden"))) CallbackLifetime {
    std::recursive_mutex gate;
    T* owner;
    bool closed{false};
    size_t borrowers{0};
    std::function<void()> dispose;
public:
    explicit CallbackLifetime(T* owner) : owner(owner) {}

    template<typename Action>
    bool invoke(Action action) {
        std::unique_lock<std::recursive_mutex> lock{gate};
        if (closed)
            return false;
        ++borrowers;
        struct Completion {
            CallbackLifetime& state;
            std::unique_lock<std::recursive_mutex>& lock;
            ~Completion() {
                --state.borrowers;
                if (state.borrowers == 0 && state.dispose) {
                    auto destroyOwner = std::move(state.dispose);
                    lock.unlock();
                    destroyOwner();
                }
            }
        } completion{*this, lock};
        action(*owner);
        return true;
    }

    void retire(std::function<void()> destroy) {
        std::unique_lock<std::recursive_mutex> lock{gate};
        if (closed)
            return;
        closed = true;
        if (borrowers) {
            dispose = std::move(destroy);
            return;
        }
        lock.unlock();
        destroy();
    }
};
}
