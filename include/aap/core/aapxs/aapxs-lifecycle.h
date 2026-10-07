#pragma once
#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>
#include "aap/aapxs.h"

namespace aap::xs {
// Reference cancellation service; useful to other frameworks too. None of its
// operations may run on processing. Construct with make_shared before attach();
// registrations outlive their framework owner.
class AsyncAbortRegistry : public std::enable_shared_from_this<AsyncAbortRegistry> {
    struct Subscription {
        std::recursive_mutex delivery;
        void* context;
        void (*handler)(void*, const char*);
        void abort(const char* error) {
            const std::lock_guard<std::recursive_mutex> gate(delivery);
            if (handler) handler(context, error);
        }
    };
    struct Registration {
        std::shared_ptr<AsyncAbortRegistry> registry;
        std::shared_ptr<Subscription> subscription;
    };
    std::mutex mutex;
    std::vector<std::shared_ptr<Subscription>> subscriptions;
    static void* subscribe(AAPXSInitiatorInstance* instance, void* context,
                           void (*handler)(void*, const char*)) {
        auto* registry = static_cast<AsyncAbortRegistry*>(instance->lifecycle_context);
        auto subscription = std::make_shared<Subscription>();
        subscription->context = context;
        subscription->handler = handler;
        auto registration = std::make_unique<Registration>(Registration{registry->shared_from_this(), subscription});
        const std::lock_guard<std::mutex> lock(registry->mutex);
        registry->subscriptions.push_back(subscription);
        return registration.release();
    }
    static void unsubscribe(void* token) {
        std::unique_ptr<Registration> registration(static_cast<Registration*>(token));
        auto subscription = registration->subscription;
        const std::lock_guard<std::recursive_mutex> gate(subscription->delivery);
        subscription->handler = nullptr;
        subscription->context = nullptr;
        const std::lock_guard<std::mutex> lock(registration->registry->mutex);
        auto& entries = registration->registry->subscriptions;
        entries.erase(std::remove(entries.begin(), entries.end(), subscription), entries.end());
    }
public:
    void attach(AAPXSInitiatorInstance& instance) {
        instance.lifecycle_context = this;
        instance.register_abort_handler = subscribe;
        instance.unregister_abort_handler = unsubscribe;
    }
    void abort(const char* error) {
        std::vector<std::shared_ptr<Subscription>> snapshot;
        {
            const std::lock_guard<std::mutex> lock(mutex);
            snapshot = subscriptions;
        }
        for (auto& subscription : snapshot) subscription->abort(error);
    }
};
}
