#ifndef AAP_CORE_TYPED_AAPXS_H
#define AAP_CORE_TYPED_AAPXS_H

#include <cassert>
#include <future>
#include "aap/aapxs.h"
#include "../../android-audio-plugin.h"

namespace aap::xs {
    template<typename T, typename R>
    struct WithPromise {
        T* context;
        std::promise<R>* promise;
    };

    class AAPXSDefinitionWrapper {
    protected:
        AAPXSDefinitionWrapper() {}
    public:
        virtual AAPXSDefinition& asPublic() = 0;
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
            auto callbackData = (WithPromise<TypedAAPXS, T>*) callbackContext;
            auto thiz = (TypedAAPXS*) callbackData->context;
            T result = *(T*) (thiz->serialization->data);
            callbackData->promise->set_value(result);
        }

        template<typename T>
        static T getTypedResult(AAPXSSerializationContext* serialization) {
            return *(T*) (serialization->data);
        }

        static void getVoidCallback(void* callbackContext, void* pluginOrHost, int32_t requestId) {
            auto callbackData = (WithPromise<TypedAAPXS, int32_t>*) callbackContext;
            callbackData->promise->set_value(0); // dummy result
        }

        // This must be visible to consuming code i.e. defined in this header file.
        // FIXME: use spinlock instead of promise<T> for RT-safe extension functions,
        //  which means there should be another RT-safe version of this function.
        template<typename T>
        T callTypedFunctionSynchronously(int32_t opcode) {
            std::promise<T> promise{};
            uint32_t requestId = aapxs_instance->get_new_request_id(aapxs_instance);
            auto future = promise.get_future();
            WithPromise<TypedAAPXS, T> callbackData{this, &promise};
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
            WithPromise<TypedAAPXS, int32_t> callbackData{this, &promise};
            AAPXSRequestContext request{getVoidCallback, &callbackData, serialization, urid, uri, requestId, opcode};

            if (aapxs_instance->send_aapxs_request(aapxs_instance, &request))
                future.wait();
        }
    };

}

#endif //AAP_CORE_TYPED_AAPXS_H
