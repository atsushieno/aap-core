#include <atomic>
#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include "aapxs-transport.h"
using aap::internal::AAPXSBinderChannel;
using namespace std::chrono_literals;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Request {
    int data{42};
    AAPXSSerializationContext serialization{&data, sizeof(data), sizeof(data)};
    std::atomic<int> callbacks{0};
    std::function<void()> onReply;
    AAPXSRequestContext request{[](void* c, void*) { static_cast<Request*>(c)->finish(); },
        this, &serialization, 1, "urn:aap:binder-test", 0, 1,
        [](void* c, void*, const char*) { static_cast<Request*>(c)->finish(); }};
    void finish() { ++callbacks; if (onReply) onReply(); }
};
static void failedDispatchAfterAbort() {
    int sharedValue = 0;
    AAPXSSerializationContext shared{&sharedValue, 0, sizeof(sharedValue)};
    Request a, b, c, replacement;
    AAPXSRequestContext first{}, second{}, fresh{};
    int sends = 0;
    std::shared_ptr<AAPXSBinderChannel> channel;
    channel = std::make_shared<AAPXSBinderChannel>(&shared, [&](const auto& routed) {
        ++sends;
        if (sends == 1) first = routed;
        if (sends == 2) {
            second = routed;
            channel->abort("abort during dispatch", nullptr);
            require(channel->send(&replacement.request), "replacement accepted");
            return false;
        }
        if (sends == 3) fresh = routed;
        return true;
    });
    require(channel->send(&a.request), "first accepted");
    require(channel->send(&b.request), "second queued");
    require(channel->send(&c.request), "third queued");
    first.callback(first.callback_user_data, nullptr);
    require(a.callbacks == 1 && b.callbacks == 1 && c.callbacks == 1,
            "dispatch failure and abort must deliver each request once");
    fresh.callback(fresh.callback_user_data, nullptr);
    require(replacement.callbacks == 1, "old queue advance must retain replacement");
    std::weak_ptr<AAPXSBinderChannel> released = channel;
    channel.reset();
    require(released.expired(), "finished replacement must release its channel");
}
static void abortWaitsForDelivery() {
    int sharedValue = 0;
    AAPXSSerializationContext shared{&sharedValue, 0, sizeof(sharedValue)};
    AAPXSRequestContext routed{};
    auto channel = std::make_shared<AAPXSBinderChannel>(&shared, [&](const auto& request) {
        routed = request; return true;
    });
    Request request;
    std::promise<void> entered, release, abortStarted;
    auto canReturn = release.get_future().share();
    request.onReply = [&] { entered.set_value(); canReturn.wait(); };
    channel->send(&request.request);
    auto completion = std::async(std::launch::async, [&] {
        routed.callback(routed.callback_user_data, nullptr);
    });
    entered.get_future().wait();
    auto abort = std::async(std::launch::async, [&] {
        abortStarted.set_value(); channel->abort("death", nullptr);
    });
    abortStarted.get_future().wait();
    bool waited = abort.wait_for(30ms) == std::future_status::timeout;
    release.set_value();
    completion.get(); abort.get();
    require(waited, "abort must wait until a claimed callback returns");
    require(request.callbacks == 1, "claimed completion must not be aborted again");
}
static void simultaneousCompletionAndAbort() {
    for (int i = 0; i != 2000; ++i) {
        int sharedValue = 0;
        AAPXSSerializationContext shared{&sharedValue, 0, sizeof(sharedValue)};
        AAPXSRequestContext routed{};
        auto channel = std::make_shared<AAPXSBinderChannel>(&shared, [&](const auto& request) {
            routed = request; return true;
        });
        Request request;
        channel->send(&request.request);
        std::atomic<bool> start{false};
        std::thread completion([&] {
            while (!start.load()) std::this_thread::yield();
            routed.callback(routed.callback_user_data, nullptr);
        });
        std::thread abort([&] {
            while (!start.load()) std::this_thread::yield();
            channel->abort("death", nullptr);
        });
        start.store(true);
        completion.join(); abort.join();
        require(request.callbacks == 1, "concurrent completion/abort must deliver once");
    }
}
static void reentrantAbortAndLateReply() {
    int sharedValue = 0;
    AAPXSSerializationContext shared{&sharedValue, 0, sizeof(sharedValue)};
    AAPXSRequestContext routed{};
    auto channel = std::make_shared<AAPXSBinderChannel>(&shared, [&](const auto& request) {
        routed = request; return true;
    });
    Request request;
    request.onReply = [&] { channel->abort("reentrant", nullptr); };
    channel->send(&request.request);
    routed.callback(routed.callback_user_data, nullptr);
    require(request.callbacks == 1, "reentrant abort must not deadlock or duplicate delivery");
    request.onReply = {};
    channel->send(&request.request);
    channel->abort("death", nullptr);
    request.serialization.data = nullptr; // callback storage may already be released
    routed.callback(routed.callback_user_data, nullptr);
    require(request.callbacks == 2, "late reply must not touch aborted buffer or deliver again");
}
static void nestedBlockingRequest() {
    int sharedValue = 0;
    AAPXSSerializationContext shared{&sharedValue, 0, sizeof(sharedValue)};
    AAPXSRequestContext first{};
    int sends = 0;
    auto channel = std::make_shared<AAPXSBinderChannel>(&shared, [&](const auto& request) {
        if (++sends == 1) first = request;
        else request.callback(request.callback_user_data, nullptr);
        return true;
    });
    Request outer, queued, nested;
    nested.request.callback = nullptr;
    outer.onReply = [&] {
        require(!channel->send(&nested.request), "nested synchronous send completes inline");
    };
    channel->send(&outer.request);
    channel->send(&queued.request);
    auto completion = std::async(std::launch::async, [&] {
        first.callback(first.callback_user_data, nullptr);
    });
    if (completion.wait_for(1s) != std::future_status::ready) {
        // Release the nested wait before reporting a regression, so futures can join.
        channel->abort("test cleanup", nullptr);
        completion.get();
        throw std::runtime_error("blocking request from a callback must make progress");
    }
    completion.get();
    require(outer.callbacks == 1 && queued.callbacks == 1 && sends == 3,
            "nested send preserves queued-request progress");
    std::weak_ptr<AAPXSBinderChannel> released = channel;
    channel.reset();
    require(released.expired(), "finished nested delivery chain must release its channel");
}
static void abortWaitsForOuterDelivery() {
    int sharedValue = 0;
    AAPXSSerializationContext shared{&sharedValue, 0, sizeof(sharedValue)};
    AAPXSRequestContext first{};
    int sends = 0;
    auto channel = std::make_shared<AAPXSBinderChannel>(&shared, [&](const auto& request) {
        if (++sends == 1) first = request;
        else request.callback(request.callback_user_data, nullptr);
        return true;
    });
    Request outer, nested;
    std::promise<void> entered, release, abortStarted;
    auto canReturn = release.get_future().share();
    outer.onReply = [&] {
        channel->send(&nested.request); // its synchronous completion has returned
        entered.set_value(); canReturn.wait();
    };
    channel->send(&outer.request);
    auto completion = std::async(std::launch::async, [&] {
        first.callback(first.callback_user_data, nullptr);
    });
    entered.get_future().wait();
    auto abort = std::async(std::launch::async, [&] {
        abortStarted.set_value(); channel->abort("death", nullptr);
    });
    abortStarted.get_future().wait();
    bool waited = abort.wait_for(30ms) == std::future_status::timeout;
    release.set_value(); completion.get(); abort.get();
    require(waited && outer.callbacks == 1 && nested.callbacks == 1,
            "abort must retain an outer callback across nested queue advancement");
    std::weak_ptr<AAPXSBinderChannel> released = channel;
    channel.reset();
    require(released.expired(), "aborted nested delivery chain must release its channel");
}
int main() try {
    failedDispatchAfterAbort();
    abortWaitsForDelivery();
    simultaneousCompletionAndAbort();
    reentrantAbortAndLateReply();
    nestedBlockingRequest();
    abortWaitsForOuterDelivery();
    puts("AAPXS Binder-completion regression tests passed");
} catch (const std::exception& e) {
    fprintf(stderr, "%s\n", e.what()); return 1;
}
