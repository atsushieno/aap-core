#include <cstdio>
#include <cstdlib>
#include <new>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>
#include "aap/core/aapxs/typed-aapxs.h"
// Count C++ allocations only during warmed sequential requests. No transport
// fixture, collection bookkeeping, or throwing assertion contributes to it.
static bool count_allocations = false;
static size_t allocations = 0;
void* operator new(size_t size) {
    if (count_allocations) ++allocations;
    if (auto result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc{};
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete[](void* ptr) noexcept { ::operator delete(ptr); }
using namespace aap;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Client : xs::TypedAAPXS {
    using TypedAAPXS::TypedAAPXS;
    size_t pooled() { std::lock_guard<std::mutex> lock(call_pool->mutex); return call_pool->idle.size(); }
};
int main() {
    struct Context { uint32_t id{0}; std::vector<AAPXSRequestContext> pending; bool inlineReply{false}; } context;
    std::vector<uint8_t> shared(1024 * 1024);
    AAPXSSerializationContext serialization{shared.data(), 0, shared.size()};
    AAPXSInitiatorInstance initiator{&context, nullptr, &serialization, 1,
        [](auto* instance) { return ++static_cast<Context*>(instance->aapxs_context)->id; },
        [](auto* instance, auto* request) {
            auto& state = *static_cast<Context*>(instance->aapxs_context);
            if (state.inlineReply) { request->callback(request->callback_user_data, nullptr); return false; }
            state.pending.push_back(*request); return true;
        }};
    auto client = std::make_unique<Client>("urn:aap:pool", &initiator, &serialization);
    std::set<void*> calls, buffers;
    int callbacks = 0;
    uint32_t value = 42;
    auto submit = [&] {
        client->callFunctionAsync(1, &value, sizeof(value), [&](const auto& error, auto* data, void*) {
            check(error.empty() && *static_cast<uint32_t*>(data->data) == value, "own reply retained"); ++callbacks;
        });
    };
    for (int i = 0; i < 200; ++i) {
        submit(); auto request = context.pending.back(); context.pending.clear();
        calls.insert(request.callback_user_data); buffers.insert(request.serialization->data);
        request.callback(request.callback_user_data, nullptr);
    }
    check(callbacks == 200 && calls.size() == 1 && buffers.size() == 1, "request object and 1 MiB buffer reused");
    context.inlineReply = true;
    auto warmSubmit = [&] {
        client->callFunctionAsync(1, &value, sizeof(value), [&callbacks](const auto& error, auto* data, void*) {
            check(error.empty() && *static_cast<uint32_t*>(data->data) == 42, "direct result delivery"); ++callbacks;
        }, sizeof(value));
    };
    warmSubmit();
    count_allocations = true;
    for (int i = 0; i < 100; ++i) warmSubmit();
    count_allocations = false;
    check(allocations == 100, "one map-node allocation per warmed request; no callback wrapper allocation");
    context.inlineReply = false;
    // No handler is also valid for a locally refused oversized request.
    client->callFunctionAsync(1, nullptr, 0, {}, 0);
    auto noHandler = context.pending.back(); context.pending.clear();
    noHandler.callback(noHandler.callback_user_data, nullptr);
    std::vector<uint8_t> oversized(shared.size() + 1);
    client->callFunctionAsync(1, oversized.data(), oversized.size(), {}, 0);
    check(context.pending.empty(), "oversized payload without callback rejected locally");
    for (int i = 0; i < 12; ++i) submit();
    calls.clear();
    for (auto& request : context.pending) calls.insert(request.serialization->data);
    check(calls.size() == 12, "concurrent requests own separate buffers");
    for (auto& request : context.pending) request.callback(request.callback_user_data, nullptr);
    context.pending.clear();
    check(client->pooled() == 2, "idle pool bounded by two MiB as well as eight entries");
    int destroyed = 0;
    client->callFunctionAsync(1, &value, 4, [&](const auto& error, auto*, void*) { check(!error.empty(), "detached owner reports error"); ++destroyed; }, 4);
    auto late = context.pending.back(); context.pending.clear();
    client.reset(); check(destroyed == 1, "destruction completes pending once");
    late.callback(late.callback_user_data, nullptr);
    check(destroyed == 1, "late callback safely reclaims detached storage and pool");
    client = std::make_unique<Client>("urn:aap:pool", &initiator, &serialization);
    context.inlineReply = true;
    client->callFunctionAsync(1, &value, 4, [&](const auto& error, auto*, void*) {
        check(error.empty(), "inline result"); client.reset();
    }, 4);
    check(!client, "inline self-destruction survives a false send return");
    puts("PASS: bounded reuse, one allocation per warmed request, concurrent ownership, detached delivery and inline destruction");
}
