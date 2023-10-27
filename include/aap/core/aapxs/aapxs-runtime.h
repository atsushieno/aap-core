#ifndef AAP_CORE_AAPXS_RUNTIME_H
#define AAP_CORE_AAPXS_RUNTIME_H

// The new 2023 version of AAPXS runtime - strongly typed.

#include <cstdint>
#include "../../unstable/aapxs-vnext.h"
#include "../../android-audio-plugin.h"

namespace aap {

    template<typename T, typename R>
    struct WithPromise {
        T* context;
        std::promise<R>* promise;
    };

    class AAPXSClientSerializer {
        void serializePluginRequest(AAPXSSerializationContext *context);
    };

    class AAPXSServiceSerializer {
        void serializePluginReply(AAPXSSerializationContext *context);
    };

    typedef struct AAPXSClientHandler {
        AAPXSProxyContextVNext
        (*as_plugin_proxy)(AAPXSClientHandler *handler, AAPXSClientSerializer *serializer);

        void (*on_host_invoked)(AAPXSClientHandler *handler, AAPXSClientSerializer *serializer,
                                AndroidAudioPluginHost *host, int32_t opcode);
    } AAPXSClientHandler;

    typedef struct AAPXSServiceHandler {
        AAPXSProxyContextVNext
        (*as_host_proxy)(AAPXSServiceHandler *handler, AAPXSServiceSerializer *serializer);

        void (*on_plugin_invoked)(AAPXSServiceHandler *handler, AAPXSServiceSerializer *serializer,
                                  AndroidAudioPlugin *plugin, int32_t opcode);
    } AAPXSServiceHandler;

    typedef struct AAPXSInstanceManager {
        void *host_context;

        AAPXSClientSerializer *(*get_client_serializer)();

        AAPXSServiceSerializer *(*get_service_serializer)();

        AAPXSClientHandler *(*get_client_handler)();

        AAPXSServiceHandler *(*get_service_handler)();
    } AAPXSInstanceManager;

    class AAPXSClientManager {
        AAPXSClientHandler* get(const char* uri);
    };

    class AAPXSFeatureRegistry {
        void add(AAPXSFeatureVNext* feature);
    };
}

#endif //AAP_CORE_AAPXS_RUNTIME_H
