#ifndef AAP_CORE_TYPED_AAPXS_H
#define AAP_CORE_TYPED_AAPXS_H

#include <future>
#include <functional>
#include <map>
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <type_traits>
#include <cstdint>
#include <string>
#include <cstring>
#include "aap/aapxs.h"
#include "aap/core/realtime.h"
#include "../../android-audio-plugin.h"
#include "aap/unstable/utility.h"
#include "result.h"

// Default per-request timeout for asynchronous AAPXS calls (and the wait bound for the
// blocking-sync wrappers built on top of them). Hardcoded default; overridable per TypedAAPXS.
#ifndef AAPXS_REQUEST_TIMEOUT_DEFAULT_MS
#define AAPXS_REQUEST_TIMEOUT_DEFAULT_MS 1000
#endif

namespace aap { class PluginInstance; }

namespace aap::xs {
    class TypedAAPXS;

    class TypedAAPXS {
        static thread_local unsigned blocking_depth;
        const char* uri;
    protected:
        AAPXSInitiatorInstance *aapxs_instance;
        AAPXSSerializationContext *serialization;
        void* abort_registration{};
        void (*unregister_abort)(void*){};
        std::shared_ptr<std::atomic<TypedAAPXS*>> lifetime{std::make_shared<std::atomic<TypedAAPXS*>>(this)};

    public:
        static bool isBlockingCall() { return blocking_depth != 0; }

        TypedAAPXS(const char* uri, AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : uri(uri), aapxs_instance(initiatorInstance), serialization(serialization) {
            if (!uri) {
                AAP_ASSERT_FALSE;
                return;
            }
            if (!aapxs_instance) {
                AAP_ASSERT_FALSE;
                return;
            }
            if (!serialization) {
                AAP_ASSERT_FALSE;
                return;
            }
            registerForAbort();
        }

        virtual ~TypedAAPXS();

        template<typename T>
        static T getTypedResult(AAPXSSerializationContext* serialization) {
            return *(T*) (serialization->data);
        }

        // Waits for a reply up to request_timeout_ms. Returns T{} on error or timeout.
        // These wrappers allocate, lock, and wait. This typed transport is not realtime safe,
        // including async completion; processing threads require a separate handoff/cached path.
        template<typename T>
        T callTypedFunctionSynchronously(int32_t opcode, const void* payload, size_t payloadSize) {
            return callAndWait<T>(opcode, payload, payloadSize,
                    [](AAPXSSerializationContext* s) { return getTypedResult<T>(s); }, sizeof(T)).value;
        }

        void callVoidFunctionSynchronously(int32_t opcode, const void* payload, size_t payloadSize) {
            (void) callAndWait<bool>(opcode, payload, payloadSize,
                    [](AAPXSSerializationContext*) { return true; }, 0);
        }

        // "Fire and forget" invocation: sends a request with no completion callback (the
        // `callback == nullptr` case). Suitable for notifications where no reply is expected.
        // NOTE: this is NOT a requirement for host extensions — service->host calls can and do
        // return values / report completion (e.g. ARA's getHostCapability / readAudioSourceSamples,
        // presets' notify*), via the same callFunctionAsync / callAndWait path as plugin extensions.
        void fireVoidFunctionAndForget(int32_t opcode) {
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            AAPXSSerializationContext empty{nullptr, 0, 0};
            AAPXSRequestContext request{nullptr, nullptr, &empty, aapxs_instance->urid, uri, requestId,
                                        opcode};
            aapxs_instance->send_aapxs_request(aapxs_instance, &request);
        }

        // ---- Asynchronous invocation (AAPXS v2 async) -------------------------------------------
        //
        // Generalized async machinery shared by every typed AAPXS that opts in (currently State).
        // A request returns immediately with a request id; the registered `deliver` closure is
        // invoked exactly once — on reply (`error` empty), timeout, or service death (`error` set).
        //
        // Every request has its own buffer (`AsyncCall::serialization`) that carries its payload and
        // receives its reply, so requests never share memory with each other; the transport copies
        // it through the extension's shared memory when it has to. Callers pass the payload in, and
        // must never write `serialization->data` themselves.
    protected:
        int32_t request_timeout_ms{AAPXS_REQUEST_TIMEOUT_DEFAULT_MS};

        struct CallPool;
        struct AsyncCall {
            std::shared_ptr<CallPool> pool;
            std::atomic<unsigned> references{1};
            std::atomic<TypedAAPXS*> owner{nullptr};
            uint32_t request_id{0};
            std::atomic<bool> fired{false};
            std::atomic<bool> detached{false};
            std::vector<uint8_t> buffer{};
            AAPXSSerializationContext serialization{};
            // error empty == success; the closure reads `serialization` only on success.
            std::function<void(const std::string& error, AAPXSSerializationContext* ctx, void* pluginOrHost)> deliver{};
        };

        // Completed requests retain their allocation in a bounded per-client pool.
        // Detached requests keep the pool alive until their transport callback;
        // idle calls never reference the pool, avoiding an ownership cycle.
        struct CallPool {
            std::mutex mutex;
            std::vector<std::unique_ptr<AsyncCall>> idle;
            size_t idle_bytes{0};
        };
        static void releaseCall(AsyncCall* call) noexcept {
            if (!call || call->references.fetch_sub(1) != 1) return;
            auto pool = std::move(call->pool);
            call->deliver = {};
            if (pool && call->buffer.capacity() <= 1024 * 1024) {
                try {
                    std::lock_guard<std::mutex> lock(pool->mutex);
                    if (pool->idle.size() < 8 && pool->idle_bytes + call->buffer.capacity() <= 2 * 1024 * 1024) {
                        pool->idle.emplace_back(call);
                        pool->idle_bytes += call->buffer.capacity();
                        return;
                    }
                } catch (...) { /* reclamation still owns the request */ }
            }
            delete call;
        }
        struct CallDeleter {
            void operator()(AsyncCall* call) const noexcept {
                if (call) call->owner.store(nullptr);
                releaseCall(call);
            }
        };
        using CallPtr = std::unique_ptr<AsyncCall, CallDeleter>;
        std::shared_ptr<CallPool> call_pool = std::make_shared<CallPool>();

        using ResultHandler = std::function<void(const std::string& error, AAPXSSerializationContext* ctx, void* pluginOrHost)>;

        std::mutex calls_mutex{};
        std::map<uint32_t, CallPtr> in_flight{};

        static void onAsyncReply(void* ctx, void* pluginOrHost) {
            auto call = (AsyncCall*) ctx;
            auto owner = call->owner.load();
            if (!owner) {
                if (call->detached.exchange(false))
                    CallDeleter{}(call);
                return;
            }
            owner->finish(call, "", pluginOrHost);
        }
        static void onAsyncError(void* ctx, void* pluginOrHost, const char* error) {
            auto call = (AsyncCall*) ctx;
            auto owner = call->owner.load();
            if (!owner) {
                if (call->detached.exchange(false))
                    CallDeleter{}(call);
                return;
            }
            owner->finish(call, error ? error : "error", pluginOrHost);
        }

        // `replyCapacity` bounds the reply that is kept (and copied back from shared memory); it is
        // clamped to the extension's capacity.
        CallPtr makeCall(const void* payload, size_t payloadSize, size_t replyCapacity, ResultHandler onResult) {
            CallPtr call;
            {
                std::lock_guard<std::mutex> lock(call_pool->mutex);
                if (!call_pool->idle.empty()) {
                    call_pool->idle_bytes -= call_pool->idle.back()->buffer.capacity();
                    call.reset(call_pool->idle.back().release());
                    call_pool->idle.pop_back();
                }
            }
            if (!call) call.reset(new AsyncCall());
            call->pool = call_pool;
            call->references.store(1);
            call->fired.store(false);
            call->detached.store(false);
            call->owner.store(this);
            call->request_id = aapxs_instance->get_new_request_id(aapxs_instance);
            call->deliver = std::move(onResult);
            auto capacity = std::max(payloadSize, std::min(replyCapacity, serialization->data_capacity));
            call->buffer.resize(capacity);
            if (payloadSize > 0)
                memcpy(call->buffer.data(), payload, payloadSize);
            call->serialization = AAPXSSerializationContext{call->buffer.data(), payloadSize, capacity};
            return call;
        }

        int32_t send(int32_t opcode, CallPtr call) {
            uint32_t requestId = call->request_id;
            AsyncCall* raw = call.get();
            // An inline completion may destroy this client or submit another
            // request before send_aapxs_request returns. Keep this exact storage
            // out of the pool until the sender has stopped borrowing it.
            ++raw->references;
            struct Borrow { AsyncCall* call; ~Borrow() { releaseCall(call); } } borrow{raw};
            if (call->serialization.data_size > serialization->data_capacity) {
                if (call->deliver)
                    call->deliver("request payload exceeds the AAPXS shared memory capacity", &call->serialization, nullptr);
                return requestId;
            }
            {
                std::lock_guard<std::mutex> lock(calls_mutex);
                in_flight[requestId] = std::move(call);
            }
            AAPXSRequestContext request{onAsyncReply, raw, &raw->serialization, aapxs_instance->urid,
                                        uri, requestId, opcode, onAsyncError};
            if (!aapxs_instance->send_aapxs_request(aapxs_instance, &request) && raw->owner.load() == this)
                finishMatchingCall(raw, "request could not be sent", requestId, true);
            return requestId;
        }

        void finish(AsyncCall* call, const std::string& error, void* pluginOrHost = nullptr) {
            finishMatchingCall(call, error, 0, false, pluginOrHost);
        }

        void finishMatchingCall(AsyncCall* call, const std::string& error, uint32_t requestId, bool matchId, void* pluginOrHost = nullptr) {
            CallPtr completing;
            {
                std::lock_guard<std::mutex> lock(calls_mutex);
                // Compare addresses before dereferencing: a failure snapshot may have been
                // completed already. Remove ownership before invoking reentrant user code.
                auto it = matchId ? in_flight.find(requestId) :
                        std::find_if(in_flight.begin(), in_flight.end(), [call](auto& entry) {
                            return entry.second.get() == call;
                        });
                if (it == in_flight.end() || it->second.get() != call || it->second->fired.exchange(true))
                    return;
                completing = std::move(it->second);
                in_flight.erase(it);
            }
            if (completing->deliver)
                completing->deliver(error, &completing->serialization, pluginOrHost);
        }

        // Defined out of line to keep transport/session internals out of the public header.
        void cancelPendingTransportRequests(const std::string& error);

        void detachAllPending(const std::string& error) {
            cancelPendingTransportRequests(error);
            std::vector<CallPtr> pending;
            {
                std::unique_lock<std::mutex> lock(calls_mutex);
                pending.reserve(in_flight.size());
                for (auto& kv : in_flight) {
                    kv.second->owner.store(nullptr);
                    kv.second->fired.exchange(true);
                    pending.push_back(std::move(kv.second));
                }
                in_flight.clear();
            }

            // In-flight calls still have a raw callback context. Complete their promises now,
            // then release ownership so the eventual callback can delete the detached context.
            for (auto& call : pending) {
                if (call->deliver)
                    call->deliver(error, &call->serialization, nullptr);
                call->detached.store(true);
                (void) call.release();
            }
        }

        // Uses only the public initiator lifecycle service; host_context is opaque.
        void registerForAbort();
        void unregisterForAbort();

    public:
        void setRequestTimeoutMs(int32_t ms) { request_timeout_ms = ms; }

        // Fail every in-flight request with `error`. Used on hard transport failure (e.g. Binder
        // service death). Cancel SysEx8 registrations and wait for any ongoing delivery before
        // releasing their request buffers; audio replies/timeouts may race the death handler.
        void failAllPending(const std::string& error) {
            auto alive = lifetime;
            cancelPendingTransportRequests(error);
            if (alive->load() != this) return;
            std::vector<std::pair<uint32_t, AsyncCall*>> pending;
            {
                std::unique_lock<std::mutex> lock(calls_mutex);
                pending.reserve(in_flight.size());
                for (auto& kv : in_flight)
                    pending.emplace_back(kv.first, kv.second.get());
            }
            for (auto& entry : pending) {
                if (alive->load() != this) break;
                finishMatchingCall(entry.second, error, entry.first, true);
            }
        }

        // Low-level async primitive. `payload` is copied, so it need not outlive this call.
        // `onResult(error, serialization, pluginOrHost)` is invoked exactly once, with the request's own buffer.
        // `replyCapacity` (defaults to the extension's capacity) bounds the reply it may read.
        int32_t callFunctionAsync(int32_t opcode, const void* payload, size_t payloadSize, ResultHandler onResult,
                                  size_t replyCapacity = SIZE_MAX) {
            // No request is accepted on a processing thread; no completion runs there.
            // -1 reports this local refusal before allocation or calls_mutex acquisition.
            if (aap::RealtimeScope::isActive()) return -1;
            return send(opcode, makeCall(payload, payloadSize, replyCapacity, std::move(onResult)));
        }

        // Blocking-sync built on top of async: waits up to `request_timeout_ms`. `deserialize`
        // runs inside the completion and produces the success value. It is skipped once we have
        // timed out, so it may capture the caller's locals by reference.
        template<typename R>
        Result<R> callAndWait(int32_t opcode, const void* payload, size_t payloadSize,
                              std::function<R(AAPXSSerializationContext*)> deserialize,
                              size_t replyCapacity = SIZE_MAX) {
            if (aap::RealtimeScope::isActive()) return {R{}, "RT caller"};
            enum : int { PENDING, DELIVERING, ABANDONED };
            struct Waiter {
                std::atomic<int> state{PENDING};
                std::promise<Result<R>> promise{};
            };
            const auto timeoutMs = request_timeout_ms;
            auto waiter = std::make_shared<Waiter>();
            auto future = waiter->promise.get_future();
            {
                struct BlockingScope {
                    BlockingScope() { ++blocking_depth; }
                    ~BlockingScope() { --blocking_depth; }
                } blockingScope;
                send(opcode, makeCall(payload, payloadSize, replyCapacity, [waiter, deserialize = std::move(deserialize)](
                        const std::string& error, AAPXSSerializationContext* s, void*) {
                    int expected = PENDING;
                    if (!waiter->state.compare_exchange_strong(expected, DELIVERING))
                        return;
                    if (!error.empty())
                        waiter->promise.set_value(Result<R>{R{}, error});
                    else
                        waiter->promise.set_value(Result<R>{deserialize(s), ""});
                }));
            }
            if (future.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready)
                return future.get();
            int expected = PENDING;
            if (!waiter->state.compare_exchange_strong(expected, ABANDONED))
                return future.get(); // being delivered right now
            // Leave the request registered: the transport timeout sweep / death handler will release it.
            return Result<R>{R{}, "timeout"};
        }
    };

    template<class Client, class Service = void>
    bool initializeTypedAAPXSInitiator(AAPXSDefinition*, AAPXSInitiatorInstance* instance, bool host) {
        TypedAAPXS* client = nullptr;
        if (!host) client = new Client(instance, instance->serialization);
        else if constexpr (!std::is_void_v<Service>) client = new Service(instance, instance->serialization);
        instance->aapxs_context = client;
        instance->typed_client = client;
        return true;
    }
    inline void releaseTypedAAPXSInitiator(AAPXSDefinition*, AAPXSInitiatorInstance* instance, bool) {
        delete static_cast<TypedAAPXS*>(instance->typed_client);
        instance->typed_client = nullptr;
    }

    class AAPXSDefinitionWrapper {
    protected:
        AAPXSDefinitionWrapper() {}

    public:
        virtual AAPXSDefinition& asPublic() = 0;
    };
}

#endif //AAP_CORE_TYPED_AAPXS_H
