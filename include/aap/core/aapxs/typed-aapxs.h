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
#include <cstdint>
#include <string>
#include <cstring>
#include "aap/aapxs.h"
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

    // Shared-owned registry of a plugin instance's async-capable AAPXS clients. Held by *both* the
    // owning PluginInstance and every TypedAAPXS (via shared_ptr), so abort iteration and teardown
    // are independent of member-destruction order. (An earlier version stored the mutex directly on
    // the instance; a TypedAAPXS owned by a later-declared member outlived the mutex and crashed in
    // pthread_mutex_lock during teardown.)
    struct AsyncAbortRegistry {
        std::mutex mutex;
        std::vector<TypedAAPXS*> abortables;
    };

    class TypedAAPXS {
        const char* uri;
    protected:
        AAPXSInitiatorInstance *aapxs_instance;
        AAPXSSerializationContext *serialization;
        std::shared_ptr<AsyncAbortRegistry> abort_registry{};

    public:
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

        // Waits without a timeout (unlike callAndWait()). Returns T{} on error.
        // FIXME: use spinlock instead of promise<T> for RT-safe extension functions,
        //  which means there should be another RT-safe version of this function.
        template<typename T>
        T callTypedFunctionSynchronously(int32_t opcode, const void* payload, size_t payloadSize) {
            auto promise = std::make_shared<std::promise<T>>();
            auto future = promise->get_future();
            send(opcode, makeCall(payload, payloadSize, sizeof(T), [promise](const std::string& error, AAPXSSerializationContext* s) {
                promise->set_value(error.empty() ? getTypedResult<T>(s) : T{});
            }));
            return future.get();
        }

        void callVoidFunctionSynchronously(int32_t opcode, const void* payload, size_t payloadSize) {
            auto promise = std::make_shared<std::promise<void>>();
            auto future = promise->get_future();
            send(opcode, makeCall(payload, payloadSize, 0, [promise](const std::string&, AAPXSSerializationContext*) {
                promise->set_value();
            }));
            future.wait();
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

        struct AsyncCall {
            std::atomic<TypedAAPXS*> owner{nullptr};
            uint32_t request_id{0};
            std::atomic<bool> fired{false};
            std::atomic<bool> detached{false};
            std::vector<uint8_t> buffer{};
            AAPXSSerializationContext serialization{};
            // error empty == success; the closure reads `serialization` only on success.
            std::function<void(const std::string& error)> deliver{};
        };

        using ResultHandler = std::function<void(const std::string& error, AAPXSSerializationContext* ctx)>;

        std::mutex calls_mutex{};
        std::map<uint32_t, std::unique_ptr<AsyncCall>> in_flight{};

        static void onAsyncReply(void* ctx, void* /*pluginOrHost*/) {
            auto call = (AsyncCall*) ctx;
            auto owner = call->owner.load();
            if (!owner) {
                if (call->detached.exchange(false))
                    delete call;
                return;
            }
            owner->finish(call, "");
        }
        static void onAsyncError(void* ctx, void* /*pluginOrHost*/, const char* error) {
            auto call = (AsyncCall*) ctx;
            auto owner = call->owner.load();
            if (!owner) {
                if (call->detached.exchange(false))
                    delete call;
                return;
            }
            owner->finish(call, error ? error : "error");
        }

        // `replyCapacity` bounds the reply that is kept (and copied back from shared memory); it is
        // clamped to the extension's capacity.
        std::unique_ptr<AsyncCall> makeCall(const void* payload, size_t payloadSize, size_t replyCapacity, ResultHandler onResult) {
            auto call = std::make_unique<AsyncCall>();
            auto raw = call.get();
            call->owner.store(this);
            call->request_id = aapxs_instance->get_new_request_id(aapxs_instance);
            call->deliver = [raw, onResult = std::move(onResult)](const std::string& error) {
                if (onResult)
                    onResult(error, &raw->serialization);
            };
            auto capacity = std::max(payloadSize, std::min(replyCapacity, serialization->data_capacity));
            call->buffer.resize(capacity);
            if (payloadSize > 0)
                memcpy(call->buffer.data(), payload, payloadSize);
            call->serialization = AAPXSSerializationContext{call->buffer.data(), payloadSize, capacity};
            return call;
        }

        int32_t send(int32_t opcode, std::unique_ptr<AsyncCall> call) {
            uint32_t requestId = call->request_id;
            AsyncCall* raw = call.get();
            if (call->serialization.data_size > serialization->data_capacity) {
                call->deliver("request payload exceeds the AAPXS shared memory capacity");
                return requestId;
            }
            {
                std::lock_guard<std::mutex> lock(calls_mutex);
                in_flight[requestId] = std::move(call);
            }
            AAPXSRequestContext request{onAsyncReply, raw, &raw->serialization, aapxs_instance->urid,
                                        uri, requestId, opcode, onAsyncError};
            if (!aapxs_instance->send_aapxs_request(aapxs_instance, &request))
                finish(raw, "request could not be sent");
            return requestId;
        }

        void finish(AsyncCall* call, const std::string& error) {
            if (call->fired.exchange(true))
                return; // exactly-once: reply vs. timeout vs. death may race
            if (call->deliver)
                call->deliver(error);
            std::lock_guard<std::mutex> lock(calls_mutex);
            in_flight.erase(call->request_id); // deletes the AsyncCall; do not touch `call` afterwards
        }

        void detachAllPending(const std::string& error) {
            std::vector<std::unique_ptr<AsyncCall>> pending;
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
                    call->deliver(error);
                call->detached.store(true);
                (void) call.release();
            }
        }

        // Registers/unregisters this instance with the owning plugin instance's abort registry,
        // so a transport-level failure can fail its in-flight requests. Reaches the instance
        // generically via `aapxs_instance->host_context` (the PluginInstance). Defined in the .cpp
        // to keep this header free of a plugin-instance.h include (which would be circular).
        void registerForAbort();
        void unregisterForAbort();

    public:
        void setRequestTimeoutMs(int32_t ms) { request_timeout_ms = ms; }

        // Fail every in-flight request with `error`. Used on hard transport failure (e.g. Binder
        // service death) where no reply will ever arrive. Safe because the death handler then
        // becomes the sole completer (Binder `completed()` cannot fire again).
        void failAllPending(const std::string& error) {
            std::vector<AsyncCall*> pending;
            {
                std::unique_lock<std::mutex> lock(calls_mutex);
                pending.reserve(in_flight.size());
                for (auto& kv : in_flight)
                    pending.push_back(kv.second.get());
            }
            for (auto* call : pending)
                finish(call, error);
        }

        // Low-level async primitive. `payload` is copied, so it need not outlive this call.
        // `onResult(error, serialization)` is invoked exactly once, with the request's own buffer.
        // `replyCapacity` (defaults to the extension's capacity) bounds the reply it may read.
        int32_t callFunctionAsync(int32_t opcode, const void* payload, size_t payloadSize, ResultHandler onResult,
                                  size_t replyCapacity = SIZE_MAX) {
            return send(opcode, makeCall(payload, payloadSize, replyCapacity, std::move(onResult)));
        }

        // Blocking-sync built on top of async: waits up to `request_timeout_ms`. `deserialize`
        // runs inside the completion and produces the success value. It is skipped once we have
        // timed out, so it may capture the caller's locals by reference.
        template<typename R>
        Result<R> callAndWait(int32_t opcode, const void* payload, size_t payloadSize,
                              std::function<R(AAPXSSerializationContext*)> deserialize,
                              size_t replyCapacity = SIZE_MAX) {
            enum : int { PENDING, DELIVERING, ABANDONED };
            struct Waiter {
                std::atomic<int> state{PENDING};
                std::promise<Result<R>> promise{};
            };
            auto waiter = std::make_shared<Waiter>();
            auto future = waiter->promise.get_future();
            send(opcode, makeCall(payload, payloadSize, replyCapacity, [waiter, deserialize = std::move(deserialize)](
                    const std::string& error, AAPXSSerializationContext* s) {
                int expected = PENDING;
                if (!waiter->state.compare_exchange_strong(expected, DELIVERING))
                    return;
                if (!error.empty())
                    waiter->promise.set_value(Result<R>{R{}, error});
                else
                    waiter->promise.set_value(Result<R>{deserialize(s), ""});
            }));
            if (future.wait_for(std::chrono::milliseconds(request_timeout_ms)) == std::future_status::ready)
                return future.get();
            int expected = PENDING;
            if (!waiter->state.compare_exchange_strong(expected, ABANDONED))
                return future.get(); // being delivered right now
            // Leave the request registered: the transport timeout sweep / death handler will release it.
            return Result<R>{R{}, "timeout"};
        }
    };

    class AAPXSDefinitionWrapper {
    protected:
        AAPXSDefinitionWrapper() {}

        std::unique_ptr<TypedAAPXS> typed_client{nullptr};
        std::unique_ptr<TypedAAPXS> typed_service{nullptr};
        AAPXSExtensionClientProxy client_proxy;
        AAPXSExtensionServiceProxy service_proxy;
    public:
        virtual AAPXSDefinition& asPublic() = 0;
    };
}

#endif //AAP_CORE_TYPED_AAPXS_H
