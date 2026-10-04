#include <atomic>
#include <cstdio>
#include <cstring>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>
#include "recipient-aapxs.h"
#include "aap/core/realtime.h"

using namespace aap::internal;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
std::shared_ptr<aap::xs::DeferredAAPXSReply> make(RecipientRequestStore& store, uint32_t id = 0) {
    uint32_t payload = 42;
    char uri[] = "urn:aap:deferred";
    AAPXSSerializationContext data{&payload, 4, 4};
    AAPXSRequestContext input{nullptr, nullptr, &data, 1, uri, id, 7};
    return store.create(input, 16);
}
void ownershipAndCompletion() {
    RecipientRequestStore store;
    std::atomic<int> calls{0};
    store.setSender([&](const auto& request) {
        check(!aap::RealtimeScope::isActive(), "reply publication off processing");
        check(request.request_id == 0 && request.opcode == 7 && request.urid == 1, "original wire identity");
        check(!strcmp(request.uri, "urn:aap:deferred"), "owned URI");
        check(request.serialization->data_size == 4 && *static_cast<uint32_t*>(request.serialization->data) == 99, "owned late reply");
        ++calls; return true;
    });
    auto original = make(store);
    auto retained = aap::xs::retainAAPXSReply(&original->request());
    check(retained == original, "retain explicit incoming context");
    original.reset(); // handler and all input/URI/parser storage have gone
    auto& request = retained->request();
    check(*static_cast<uint32_t*>(request.serialization->data) == 42, "payload survived handler return");
    *static_cast<uint32_t*>(request.serialization->data) = 99;
    std::vector<std::thread> replies;
    std::atomic<int> successes{0};
    for (int i = 0; i < 8; ++i) replies.emplace_back([&] { if (retained->complete()) ++successes; });
    for (auto& thread : replies) thread.join();
    check(calls == 1 && successes == 1, "concurrent completion publishes once");
    check(!retained->complete(), "duplicate completion ignored");
    store.close();
    check(!retained->complete(), "completed handle safe after close");
    check(request.serialization->data_size == 4, "storage owned by retained handle after close");
    AAPXSRequestContext borrowed{};
    check(!aap::xs::retainAAPXSReply(&borrowed), "borrowed Binder request not retainable");
}
void backpressureAndTeardown() {
    RecipientRequestStore store;
    bool accept = false;
    store.setSender([&](const auto&) { return accept; });
    auto reply = make(store);
    check(!reply->complete(), "publication backpressure reported");
    accept = true;
    check(reply->complete(), "retained reply retries after backpressure");
    reply = make(store);
    store.close();
    check(!reply->complete(), "late completion rejected without accessing dead target");
    check(*static_cast<uint32_t*>(reply->request().serialization->data) == 42, "late payload remains owned");
    check(!make(store), "closed store rejects dispatch");
}
void boundsAndRealtimeRefusal() {
    RecipientRequestStore store;
    store.setSender([](const auto&) { return true; });
    std::vector<std::shared_ptr<aap::xs::DeferredAAPXSReply>> replies;
    for (int i = 0; i < 255; ++i) { replies.push_back(make(store, i)); check(bool(replies.back()), "recipient capacity available"); }
    check(!make(store, 255), "recipient limit rejects");
    check(replies[0]->complete(), "recipient capacity released");
    check(bool(make(store, 255)), "recipient capacity reusable");
    auto& context = replies[1]->request();
    context.serialization->data_size = 17;
    check(!replies[1]->complete(), "oversized reply rejected");
    context.serialization->data_size = 4;
    {
        aap::RealtimeScope rt;
        check(!aap::xs::retainAAPXSReply(&context), "retain refuses processing before registry lock");
        check(!replies[1]->complete(), "complete refuses processing before state lock");
        check(!make(store), "dispatch refuses processing");
    }
    check(replies[1]->complete(), "non-processing completion still works");
    replies.clear();
    for (int i = 0; i < 600; ++i)
        check(bool(make(store, i)), "abandoned handles release pending storage/capacity");
}
void closeWaitsForSender() {
    RecipientRequestStore store;
    std::promise<void> entered, release;
    auto resume = release.get_future().share();
    std::atomic<bool> closed{false};
    store.setSender([&](const auto&) { entered.set_value(); resume.wait(); check(!closed, "target lives throughout sender"); return true; });
    auto reply = make(store);
    auto sender = std::async(std::launch::async, [&] { return reply->complete(); });
    entered.get_future().wait();
    auto closer = std::async(std::launch::async, [&] { store.close(); closed = true; });
    check(closer.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout, "close waits for publishing sender");
    release.set_value();
    check(sender.get(), "in-progress publication completes"); closer.get();
    check(!reply->complete(), "late completion after close is inert");
}
int main() {
    ownershipAndCompletion(); backpressureAndTeardown(); boundsAndRealtimeRefusal(); closeWaitsForSender();
    puts("PASS: deferred recipient ownership, exactly-once replies, backpressure and teardown");
}
