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
#include <chrono>
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
    template<typename T, typename R>
    struct WithPromise {
        void* data;
        std::promise<R>* promise;
    };

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

        // This must be visible to consuming code i.e. defined in this header file.
        template<typename T>
        static void getTypedCallback(void* callbackContext, void* pluginOrHost) {
            auto callbackData = (WithPromise<void*, T>*) callbackContext;
            T result = *(T*) (callbackData->data);
            callbackData->promise->set_value(result);
        }

        template<typename T>
        static T getTypedResult(AAPXSSerializationContext* serialization) {
            return *(T*) (serialization->data);
        }

        static void getVoidCallback(void* callbackContext, void* pluginOrHost) {
            auto callbackData = (WithPromise<void*, int32_t>*) callbackContext;
            callbackData->promise->set_value(0); // dummy result, just signaling the std::future
        }

        // This must be visible to consuming code i.e. defined in this header file.
        // FIXME: use spinlock instead of promise<T> for RT-safe extension functions,
        //  which means there should be another RT-safe version of this function.
        template<typename T>
        T callTypedFunctionSynchronously(int32_t opcode) {
            std::promise<T> promise{};
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            auto future = promise.get_future();
            WithPromise<void, T> callbackData{serialization->data, &promise};
            AAPXSRequestContext request{getTypedCallback<T>, &callbackData, serialization, aapxs_instance->urid, uri, requestId, opcode};

            if (aapxs_instance->send_aapxs_request(aapxs_instance, &request)) {
                future.wait();
                return future.get();
            }
            else
                return getTypedResult<T>(serialization);
        }

        // FIXME: use spinlock instead of promise<T> for RT-safe extension functions,
        //  which means there should be another RT-safe version of this function.
        void callVoidFunctionSynchronously(int32_t opcode) {
            std::promise<int32_t> promise{};
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            WithPromise<void, int32_t> callbackData{serialization->data, &promise};
            auto future = promise.get_future();
            AAPXSRequestContext request{getVoidCallback, &callbackData, serialization, aapxs_instance->urid, uri, requestId, opcode};

            if (aapxs_instance->send_aapxs_request(aapxs_instance, &request))
                future.wait();
        }

        // "Fire and forget" invocation: sends a request with no completion callback (the
        // `callback == nullptr` case). Suitable for notifications where no reply is expected.
        // NOTE: this is NOT a requirement for host extensions — service->host calls can and do
        // return values / report completion (e.g. ARA's getHostCapability / readAudioSourceSamples,
        // presets' notify*), via the same callFunctionAsync / callAndWait path as plugin extensions.
        void fireVoidFunctionAndForget(int32_t opcode) {
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            AAPXSRequestContext request{nullptr, nullptr, serialization, aapxs_instance->urid, uri, requestId,
                                        opcode};
            aapxs_instance->send_aapxs_request(aapxs_instance, &request);
        }

        // ---- Asynchronous invocation (AAPXS v2 async) -------------------------------------------
        //
        // Generalized async machinery shared by every typed AAPXS that opts in (currently State).
        // A request returns immediately with a request id; the registered `deliver` closure is
        // invoked exactly once — on reply (`error` empty), timeout, or service death (`error` set).
        //
        // Shared-memory safety (N=1): each extension instance owns a single serialization block.
        // While a request is in flight, further requests are *deferred* (not dropped, not blocked):
        // their payload is snapshotted and replayed when the block frees up. With per-plugin-instance
        // blocks this only ever serializes the rare same-instance/same-extension overlap.
    protected:
        int32_t request_timeout_ms{AAPXS_REQUEST_TIMEOUT_DEFAULT_MS};

        struct AsyncCall {
            std::atomic<TypedAAPXS*> owner{nullptr};
            uint32_t request_id{0};
            std::atomic<bool> fired{false};
            std::atomic<bool> detached{false};
            // error empty == success; the closure reads `serialization` only on success.
            std::function<void(const std::string& error)> deliver{};
        };

        std::mutex calls_mutex{};
        std::map<uint32_t, std::unique_ptr<AsyncCall>> in_flight{};

        struct DeferredCall {
            int32_t opcode{0};
            std::vector<uint8_t> payload{};
            std::unique_ptr<AsyncCall> call{};
        };
        std::deque<DeferredCall> deferred{};

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

        // assumes `calls_mutex` is held on entry; releases it before doing IPC.
        int32_t sendLocked(int32_t opcode, std::unique_ptr<AsyncCall> call, std::unique_lock<std::mutex>& lock) {
            uint32_t requestId = call->request_id;
            AsyncCall* raw = call.get();
            in_flight[requestId] = std::move(call);
            AAPXSRequestContext request{onAsyncReply, raw, serialization, aapxs_instance->urid,
                                        uri, requestId, opcode, onAsyncError};
            lock.unlock();
            bool sent = aapxs_instance->send_aapxs_request(aapxs_instance, &request);
            if (!sent)
                finish(raw, "request could not be sent");
            return requestId;
        }

        void finish(AsyncCall* call, const std::string& error) {
            if (call->fired.exchange(true))
                return; // exactly-once: reply vs. timeout vs. death may race
            // Deliver (copies results out of `serialization`) before any deferred replay overwrites it.
            if (call->deliver)
                call->deliver(error);
            std::unique_lock<std::mutex> lock(calls_mutex);
            in_flight.erase(call->request_id); // deletes the AsyncCall; do not touch `call` afterwards
            if (!deferred.empty() && in_flight.empty()) {
                DeferredCall d = std::move(deferred.front());
                deferred.pop_front();
                if (!d.payload.empty())
                    memcpy(serialization->data, d.payload.data(), d.payload.size());
                serialization->data_size = d.payload.size();
                sendLocked(d.opcode, std::move(d.call), lock);
            }
        }

        void detachAllPending(const std::string& error) {
            std::vector<std::unique_ptr<AsyncCall>> pending;
            std::deque<DeferredCall> abortedDeferred;
            {
                std::unique_lock<std::mutex> lock(calls_mutex);
                pending.reserve(in_flight.size());
                for (auto& kv : in_flight) {
                    kv.second->owner.store(nullptr);
                    kv.second->fired.exchange(true);
                    pending.push_back(std::move(kv.second));
                }
                in_flight.clear();
                abortedDeferred = std::move(deferred);
                deferred.clear();
            }

            // In-flight Binder calls still have a raw callback context. Complete their promises now,
            // then release ownership so the eventual Binder callback can delete the detached context.
            for (auto& call : pending) {
                if (call->deliver)
                    call->deliver(error);
                call->detached.store(true);
                (void) call.release();
            }

            // Deferred calls were never sent to Binder, so no external callback can reference them.
            for (auto& d : abortedDeferred)
                if (d.call && !d.call->fired.exchange(true) && d.call->deliver)
                    d.call->deliver(error);
        }

        // Registers/unregisters this instance with the owning plugin instance's abort registry,
        // so a transport-level failure can fail its in-flight requests. Reaches the instance
        // generically via `aapxs_instance->host_context` (the PluginInstance). Defined in the .cpp
        // to keep this header free of a plugin-instance.h include (which would be circular).
        void registerForAbort();
        void unregisterForAbort();

    public:
        void setRequestTimeoutMs(int32_t ms) { request_timeout_ms = ms; }

        // Fail every in-flight and not-yet-sent request with `error`. Used on hard transport
        // failure (e.g. Binder service death) where no reply will ever arrive. Safe because the
        // death handler then becomes the sole completer (Binder `completed()` cannot fire again).
        void failAllPending(const std::string& error) {
            std::vector<AsyncCall*> pending;
            std::deque<DeferredCall> abortedDeferred;
            {
                std::unique_lock<std::mutex> lock(calls_mutex);
                pending.reserve(in_flight.size());
                for (auto& kv : in_flight)
                    pending.push_back(kv.second.get());
                // Clear deferred first so finish()'s pump does not replay onto a dead service.
                abortedDeferred = std::move(deferred);
                deferred.clear();
            }
            for (auto* call : pending)
                finish(call, error);
            for (auto& d : abortedDeferred)
                if (d.call && !d.call->fired.exchange(true) && d.call->deliver)
                    d.call->deliver(error);
        }

        // Low-level async primitive. `serialization->data` must already hold the request payload.
        // `onResult(error, serialization)` is invoked exactly once; it must copy out whatever it
        // needs from `serialization` (the block may be reused right after it returns).
        int32_t callFunctionAsync(int32_t opcode,
                                  std::function<void(const std::string& error, AAPXSSerializationContext* ctx)> onResult) {
            auto ctx = serialization;
            auto call = std::make_unique<AsyncCall>();
            call->owner.store(this);
            call->deliver = [ctx, onResult = std::move(onResult)](const std::string& error) {
                if (onResult)
                    onResult(error, ctx);
            };
            std::unique_lock<std::mutex> lock(calls_mutex);
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            call->request_id = requestId;
            if (!in_flight.empty()) {
                // block busy: defer with a snapshot of the current request payload.
                DeferredCall d;
                d.opcode = opcode;
                auto data = (uint8_t*) ctx->data;
                d.payload.assign(data, data + ctx->data_size);
                d.call = std::move(call);
                deferred.push_back(std::move(d));
                return requestId;
            }
            return sendLocked(opcode, std::move(call), lock);
        }

        // Blocking-sync built on top of async: waits up to `request_timeout_ms`. `deserialize`
        // runs inside the completion (safe window) and produces the success value.
        template<typename R>
        Result<R> callAndWait(int32_t opcode, std::function<R(AAPXSSerializationContext*)> deserialize) {
            auto promise = std::make_shared<std::promise<Result<R>>>();
            auto future = promise->get_future();
            callFunctionAsync(opcode, [promise, deserialize = std::move(deserialize)](
                    const std::string& error, AAPXSSerializationContext* s) {
                if (!error.empty())
                    promise->set_value(Result<R>{R{}, error});
                else
                    promise->set_value(Result<R>{deserialize(s), ""});
            });
            if (future.wait_for(std::chrono::milliseconds(request_timeout_ms)) == std::future_status::ready)
                return future.get();
            // Leave the request registered: the transport timeout sweep / death handler will
            // release it (and the shared promise keeps the late set_value harmless).
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
