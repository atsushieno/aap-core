#ifndef AAP_CORE_TYPED_AAPXS_H
#define AAP_CORE_TYPED_AAPXS_H

#include <cassert>
#include <future>
#include "aap/aapxs.h"
#include "../../android-audio-plugin.h"

namespace aap::xs {
    template<typename T, typename R>
    struct WithPromise {
        void* data;
        std::promise<R>* promise;
    };

    class TypedAAPXS {
        uint8_t urid{0};
        const char* uri;
    protected:
        AAPXSInitiatorInstance *aapxs_instance;
        AAPXSSerializationContext *serialization;

    public:
        TypedAAPXS(const char* uri, AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : uri(uri), aapxs_instance(initiatorInstance), serialization(serialization) {
            assert(uri);
            assert(aapxs_instance);
            assert(serialization);
        }

        virtual ~TypedAAPXS() {}

        // This must be visible to consuming code i.e. defined in this header file.
        template<typename T>
        static void getTypedCallback(void* callbackContext, void* pluginOrHost, int32_t requestId) {
            auto callbackData = (WithPromise<void*, T>*) callbackContext;
            T result = *(T*) (callbackData->data);
            callbackData->promise->set_value(result);
        }

        template<typename T>
        static T getTypedResult(AAPXSSerializationContext* serialization) {
            return *(T*) (serialization->data);
        }

        static void getVoidCallback(void* callbackContext, void* pluginOrHost, int32_t requestId) {
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
            AAPXSRequestContext request{getTypedCallback<T>, &callbackData, serialization, urid, uri, requestId, opcode};

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
            auto future = promise.get_future();
            WithPromise<void, int32_t> callbackData{serialization->data, &promise};
            AAPXSRequestContext request{getVoidCallback, &callbackData, serialization, urid, uri, requestId, opcode};

            if (aapxs_instance->send_aapxs_request(aapxs_instance, &request))
                future.wait();
        }

        // "Fire and forget" invocation. Host extensions must be always like this.
        void fireVoidFunctionAndForget(int32_t opcode) {
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            AAPXSRequestContext request{nullptr, nullptr, serialization, urid, uri, requestId,
                                        opcode};
            aapxs_instance->send_aapxs_request(aapxs_instance, &request);
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
