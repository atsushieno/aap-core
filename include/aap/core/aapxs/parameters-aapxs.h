#ifndef AAP_CORE_PARAMETERS_AAPXS_H
#define AAP_CORE_PARAMETERS_AAPXS_H

#include <functional>
#include <future>
#include "aap/aapxs.h"
#include "../../ext/parameters.h"
#include "typed-aapxs.h"

// plugin extension opcodes
const int32_t OPCODE_PARAMETERS_GET_PARAMETER_COUNT = 1;
const int32_t OPCODE_PARAMETERS_GET_PARAMETER = 2;
const int32_t OPCODE_PARAMETERS_GET_PROPERTY = 3;
const int32_t OPCODE_PARAMETERS_GET_ENUMERATION_COUNT = 4;
const int32_t OPCODE_PARAMETERS_GET_ENUMERATION = 5;

// host extension opcodes
const int32_t OPCODE_NOTIFY_PARAMETERS_CHANGED = -1;

const int32_t PARAMETERS_SHARED_MEMORY_SIZE = sizeof(aap_parameter_info_t); // it is the expected max size in v2.1 extension.

namespace aap::xs {
    class AAPXSDefinition_Parameters : public AAPXSDefinitionWrapper {

        static void aapxs_parameters_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_parameters_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_parameters_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_parameters_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        AAPXSDefinition aapxs_parameters{AAP_PARAMETERS_EXTENSION_URI,
                                      this,
                                      PARAMETERS_SHARED_MEMORY_SIZE,
                                      aapxs_parameters_process_incoming_plugin_aapxs_request,
                                      aapxs_parameters_process_incoming_host_aapxs_request,
                                      aapxs_parameters_process_incoming_plugin_aapxs_reply,
                                      aapxs_parameters_process_incoming_host_aapxs_reply
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_parameters;
        }
    };

    class ParametersClientAAPXS : public TypedAAPXS {
        typedef void (*aapxs_async_get_parameter_callback) (aap::xs::ParametersClientAAPXS*, void * pluginOrHost, int32_t requestId, int32_t index, aap_parameter_info_t result);
        typedef void (*aapxs_async_get_enumeration_callback) (aap::xs::ParametersClientAAPXS*, void * pluginOrHost, int32_t requestId, int32_t index, int32_t enumIndex, aap_parameter_enum_t result);

        struct CallbackData {
            void* context{nullptr};
            void* callback{nullptr};
            int32_t index{0};
            int32_t enum_index{0};
        };

        CallbackData pending_calls[UINT8_MAX];
        static void completeWithParameterCallback(void *callbackData, void *pluginOrHost, int32_t requestId);
        static void completeWithEnumCallback(void *callbackData, void *pluginOrHost, int32_t requestId);

    public:
        ParametersClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PARAMETERS_EXTENSION_URI, initiatorInstance, serialization) {
            memset(pending_calls, 0, sizeof(CallbackData) * UINT8_MAX);
        }

        int32_t getParameterCount();
        aap_parameter_info_t getParameter(int32_t index);
        double getProperty(int32_t index, int32_t propertyId);
        int32_t getEnumerationCount(int32_t index);
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex);

        // returns request ID
        int32_t getParameterAsync(int32_t index, aapxs_async_get_parameter_callback* callback);
        // returns request ID
        int32_t getEnumerationAsync(int32_t index, int32_t enumIndex, aapxs_async_get_enumeration_callback* callback);
    };

    class ParametersServiceAAPXS : public TypedAAPXS {
    public:
        ParametersServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PARAMETERS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyParametersChanged();

        static void staticNotifyParametersChanged(aap_parameters_host_extension_t* ext, AndroidAudioPluginHost* host) {
            ((ParametersServiceAAPXS*) ext->aapxs_context)->notifyParametersChanged();
        }
        aap_parameters_host_extension_t host_extension{this, staticNotifyParametersChanged};

        aap_parameters_host_extension_t* asHostExtension() { return &host_extension; }
    };
}

#endif //AAP_CORE_PARAMETERS_AAPXS_H
