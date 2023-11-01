#ifndef AAP_CORE_PARAMETERS_AAPXS_H
#define AAP_CORE_PARAMETERS_AAPXS_H

#include <functional>
#include <future>
#include "aap/unstable/aapxs-vnext.h"
#include "aap/ext/parameters.h"
#include "aap/core/aapxs/aapxs-runtime.h"

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
                AAPXSRequestContext* context);
        static void aapxs_parameters_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* context);
        static void aapxs_parameters_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* context);
        static void aapxs_parameters_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* context);

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
    public:
        ParametersClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PARAMETERS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        int32_t getParameterCount();
        aap_parameter_info_t getParameter(int32_t index);
        double getProperty(int32_t index, int32_t propertyId);
        int32_t getEnumerationCount(int32_t index);
        aap_parameter_enum_t getEnumeration(int32_t index, int32_t enumIndex);
    };

    class ParametersServiceAAPXS : public TypedAAPXS {
    public:
        ParametersServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PARAMETERS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyParametersChanged();
    };
}

#endif //AAP_CORE_PARAMETERS_AAPXS_H
