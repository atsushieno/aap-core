

#ifndef AAP_CORE_URID_AAPXS_H
#define AAP_CORE_URID_AAPXS_H

#include <functional>
#include <future>
#include "aap/aapxs.h"
#include "../../ext/urid.h"
#include "typed-aapxs.h"

// plugin extension opcodes
const int32_t OPCODE_MAP = 1;

// host extension opcodes
// ... nothing?

const int32_t URID_SHARED_MEMORY_SIZE = AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t) * 2 + AAP_MAX_EXTENSION_URI_SIZE + 1;

namespace aap::xs {
    class UridClientAAPXS : public TypedAAPXS {
        static void staticMap(aap_urid_extension_t* ext, AndroidAudioPlugin*, uint8_t urid, const char* uri) {
            return ((UridClientAAPXS*) ext->aapxs_context)->map(urid, uri);
        }
        aap_urid_extension_t as_public_extension{this, staticMap};
    public:
        UridClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_URID_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void map(uint8_t urid, const char* uri);

        aap_urid_extension_t* asPluginExtension() { return &as_public_extension; }
    };

    class UridServiceAAPXS : public TypedAAPXS {
    public:
        UridServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_URID_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // nothing?
    };

    class AAPXSDefinition_Urid : public AAPXSDefinitionWrapper {

        static void aapxs_urid_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_urid_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_urid_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_urid_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        // It is used in synchronous context such as `get_extension()` in `binder-client-as-plugin.cpp` etc.
        static AAPXSExtensionClientProxy aapxs_urid_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);

        static void* aapxs_urid_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            return ((UridClientAAPXS*) proxy->aapxs_context)->asPluginExtension();
        }

        AAPXSDefinition aapxs_urid{this,
                                   AAP_URID_EXTENSION_URI,
                                   URID_SHARED_MEMORY_SIZE,
                                   aapxs_urid_process_incoming_plugin_aapxs_request,
                                   aapxs_urid_process_incoming_host_aapxs_request,
                                   aapxs_urid_process_incoming_plugin_aapxs_reply,
                                   aapxs_urid_process_incoming_host_aapxs_reply,
                                   aapxs_urid_get_plugin_proxy,
                                   nullptr // no host extension
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_urid;
        }
    };
}

#endif //AAP_CORE_URID_AAPXS_H
