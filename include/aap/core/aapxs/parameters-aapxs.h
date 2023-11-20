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
    class ParametersClientAAPXS : public TypedAAPXS {
        // extension proxy support
        static int32_t staticGetParameterCount(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ((ParametersClientAAPXS*) ext->aapxs_context)->getParameterCount();
        }
        static aap_parameter_info_t staticGetParameter(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index) {
            return ((ParametersClientAAPXS*) ext->aapxs_context)->getParameter(index);
        }
        static double staticGetProperty(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index, int32_t propertyId) {
            return ((ParametersClientAAPXS*) ext->aapxs_context)->getProperty(index, propertyId);
        }
        static int32_t staticGetEnumerationCount(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index) {
            return ((ParametersClientAAPXS*) ext->aapxs_context)->getEnumerationCount(index);
        }
        static aap_parameter_enum_t staticGetEnumeration(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index, int32_t enumIndex) {
            return ((ParametersClientAAPXS*) ext->aapxs_context)->getEnumeration(index, enumIndex);
        }
        aap_parameters_extension_t as_plugin_extension{this,
                                                       staticGetParameterCount,
                                                       staticGetParameter,
                                                       staticGetProperty,
                                                       staticGetEnumerationCount,
                                                       staticGetEnumeration};

        // async invocation support
        typedef void (*aapxs_async_get_parameter_callback) (aap::xs::ParametersClientAAPXS*, void * pluginOrHost, int32_t index, aap_parameter_info_t result);
        typedef void (*aapxs_async_get_enumeration_callback) (aap::xs::ParametersClientAAPXS*, void * pluginOrHost, int32_t index, int32_t enumIndex, aap_parameter_enum_t result);

        struct CallbackData {
            void* context{nullptr};
            void* callback{nullptr};
            int32_t index{0};
            int32_t enum_index{0};
        };

        CallbackData pending_calls[UINT8_MAX];
        static void completeWithParameterCallback(void *callbackData, void *pluginOrHost);
        static void completeWithEnumCallback(void *callbackData, void *pluginOrHost);

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

        aap_parameters_extension_t* asPluginExtension() { return &as_plugin_extension; }
    };

    class ParametersServiceAAPXS : public TypedAAPXS {
        // extension proxy support
        static void staticNotifyParametersChanged(aap_parameters_host_extension_t* ext, AndroidAudioPluginHost* host) {
            ((ParametersServiceAAPXS*) ext->aapxs_context)->notifyParametersChanged();
        }
        aap_parameters_host_extension_t host_extension{this, staticNotifyParametersChanged};

    public:
        ParametersServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_PARAMETERS_EXTENSION_URI, initiatorInstance, serialization) {
        }

        void notifyParametersChanged();

        aap_parameters_host_extension_t* asHostExtension() { return &host_extension; }
    };

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

        // It is used in synchronous context such as `get_extension()` in `binder-client-as-plugin.cpp` etc.
        static AAPXSExtensionClientProxy aapxs_parameters_get_plugin_proxy(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AAPXSSerializationContext* serialization);

        static AAPXSExtensionServiceProxy aapxs_parameters_get_host_proxy(
                struct AAPXSDefinition *feature,
                AAPXSInitiatorInstance *aapxsInstance,
                AAPXSSerializationContext *serialization);

        static void* aapxs_parameters_as_plugin_extension(AAPXSExtensionClientProxy* proxy) {
            return ((ParametersClientAAPXS*) proxy->aapxs_context)->asPluginExtension();
        }

        static void* aapxs_parameters_as_host_extension(AAPXSExtensionServiceProxy* proxy) {
            return ((ParametersServiceAAPXS*) proxy->aapxs_context)->asHostExtension();
        }

        AAPXSDefinition aapxs_parameters{this,
                                         AAP_PARAMETERS_EXTENSION_URI,
                                         PARAMETERS_SHARED_MEMORY_SIZE,
                                         aapxs_parameters_process_incoming_plugin_aapxs_request,
                                         aapxs_parameters_process_incoming_host_aapxs_request,
                                         aapxs_parameters_process_incoming_plugin_aapxs_reply,
                                         aapxs_parameters_process_incoming_host_aapxs_reply,
                                         aapxs_parameters_get_plugin_proxy,
                                         aapxs_parameters_get_host_proxy
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_parameters;
        }
    };
}

#endif //AAP_CORE_PARAMETERS_AAPXS_H
