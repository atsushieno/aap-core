#ifndef AAP_CORE_PARAMETERS_SERVICE_H
#define AAP_CORE_PARAMETERS_SERVICE_H


#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/aapxs.h"
#include "aap/ext/parameters.h"
#include "aap/unstable/logging.h"
#include "aap/core/aapxs/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

const int32_t OPCODE_PARAMETERS_GET_PARAMETER_COUNT = 1;
const int32_t OPCODE_PARAMETERS_GET_PARAMETER = 2;
const int32_t OPCODE_PARAMETERS_GET_PROPERTY = 3;
const int32_t OPCODE_PARAMETERS_GET_ENUMERATION_COUNT = 4;
const int32_t OPCODE_PARAMETERS_GET_ENUMERATION = 5;

const int32_t PARAMETERS_SHARED_MEMORY_SIZE = sizeof(aap_parameter_info_t); // it is the expected max size in v2.1 extension.

class ParametersPluginClientExtension : public PluginClientExtensionImplBase {
    class Instance {
        friend class ParametersPluginClientExtension;

        aap_parameters_extension_t proxy{};

        static int32_t internalGetParameterCount(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin) {
            return ((Instance *) ext->aapxs_context)->getParameterCount();
        }

        static aap_parameter_info_t internalGetParameter(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t index) {
            return ((Instance *) ext->aapxs_context)->getParameter(index);
        }

        static double internalGetParameterProperty(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t parameterId, int32_t propertyId) {
            return ((Instance *) ext->aapxs_context)->getParameterProperty(parameterId, propertyId);
        }

        static int32_t internalGetEnumerationCount(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t parameterId) {
            return ((Instance *) ext->aapxs_context)->getEnumerationCount(parameterId);
        }

        static aap_parameter_enum_t internalGetEnumeration(aap_parameters_extension_t* ext, AndroidAudioPlugin* plugin, int32_t parameterId, int32_t enumIndex) {
            return ((Instance *) ext->aapxs_context)->getEnumeration(parameterId, enumIndex);
        }

        ParametersPluginClientExtension *owner;
        AAPXSClientInstance* aapxsInstance;

    public:
        Instance(ParametersPluginClientExtension *owner, AAPXSClientInstance *clientInstance)
            : owner(owner)
        {
            aapxsInstance = clientInstance;
        }

        void clientInvokePluginExtension(int32_t opcode) {
            owner->clientInvokePluginExtension(aapxsInstance, opcode);
        }

        int32_t getParameterCount() {
            clientInvokePluginExtension(OPCODE_PARAMETERS_GET_PARAMETER_COUNT);
            return *((int32_t *) aapxsInstance->data);
        }

        aap_parameter_info_t getParameter(int32_t index) {
            *((int32_t *) aapxsInstance->data) = index;
            clientInvokePluginExtension(OPCODE_PARAMETERS_GET_PARAMETER);
            aap_parameter_info_t ret;
            memcpy(&ret, aapxsInstance->data, sizeof(ret));
            return ret;
        }

        double getParameterProperty(int32_t parameterId, int32_t propertyId) {
            *((int32_t *) aapxsInstance->data) = parameterId;
            *((int32_t *) aapxsInstance->data + 1) = propertyId;
            clientInvokePluginExtension(OPCODE_PARAMETERS_GET_PROPERTY);
            return *(double *) aapxsInstance->data;
        }

        int32_t getEnumerationCount(int32_t parameterId) {
            *((int32_t *) aapxsInstance->data) = parameterId;
            clientInvokePluginExtension(OPCODE_PARAMETERS_GET_ENUMERATION_COUNT);
            return *((int32_t *) aapxsInstance->data);
        }

        aap_parameter_enum_t getEnumeration(int32_t parameterId, int32_t enumIndex) {
            *((int32_t *) aapxsInstance->data) = parameterId;
            *((int32_t *) aapxsInstance->data + 1) = enumIndex;
            clientInvokePluginExtension(OPCODE_PARAMETERS_GET_ENUMERATION);
            aap_parameter_enum_t ret;
            memcpy(&ret, aapxsInstance->data, sizeof(ret));
            return ret;
        }

        AAPXSProxyContext asProxy() {
            proxy.aapxs_context = this;
            proxy.get_parameter_count = internalGetParameterCount;
            proxy.get_parameter = internalGetParameter;
            proxy.get_parameter_property = internalGetParameterProperty;
            proxy.get_enumeration_count = internalGetEnumerationCount;
            proxy.get_enumeration = internalGetEnumeration;
            return AAPXSProxyContext{aapxsInstance, this, &proxy};
        }
    };

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define PARAMETERS_AAPXS_MAX_INSTANCE_COUNT 1024

    std::unique_ptr<Instance> instances[PARAMETERS_AAPXS_MAX_INSTANCE_COUNT]{};
    std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

public:
    ParametersPluginClientExtension()
            : PluginClientExtensionImplBase() {
    }

    AAPXSProxyContext asProxy(AAPXSClientInstance *clientInstance) override {
        size_t last = 0;
        for (; last < PARAMETERS_AAPXS_MAX_INSTANCE_COUNT; last++) {
            if (instances[last] == nullptr)
                break;
            if (instances[last]->aapxsInstance == clientInstance)
                return instances[instance_map[clientInstance->plugin_instance_id]]->asProxy();
        }
        instances[last] = std::make_unique<Instance>(this, clientInstance);
        instance_map[clientInstance->plugin_instance_id] = (int32_t) last;
        return instances[last]->asProxy();
    }
};

class ParametersPluginServiceExtension : public PluginServiceExtensionImplBase {

public:
    ParametersPluginServiceExtension()
            : PluginServiceExtensionImplBase(AAP_PARAMETERS_EXTENSION_URI) {
    }

    template<typename T>
    void withParametersExtension(AndroidAudioPlugin* plugin, T defaultValue,
            std::function<void(aap_parameters_extension_t *,
                               AndroidAudioPlugin*)> func) {
        // This instance->getExtension() should return an extension from the loaded plugin.
        assert(plugin);
        auto parametersExtension = (aap_parameters_extension_t *) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
        // Note that parameterExtension may be NULL.
        // The existence of metadata registration is meaningful by itself.
        // The extension instance is needed only if it provides dynamic parameter list or properties.
        //assert(parametersExtension);
        func(parametersExtension, plugin);
    }

    // invoked by AudioPluginService
    void onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                   int32_t opcode) override {
        switch (opcode) {
            case OPCODE_PARAMETERS_GET_PARAMETER_COUNT:
                withParametersExtension<int32_t>(plugin, 0, [=](aap_parameters_extension_t *ext,
                                                                AndroidAudioPlugin* plugin) {
                    *((int32_t *) extensionInstance->data) =
                            (ext != nullptr && ext->get_parameter_count) ? ext->get_parameter_count(ext, plugin) : -1;
                    return 0;
                });
                break;
            case OPCODE_PARAMETERS_GET_PARAMETER:
                withParametersExtension<int32_t>(plugin, 0, [=](aap_parameters_extension_t *ext,
                                                                AndroidAudioPlugin* plugin) {
                    if (ext != nullptr && ext->get_parameter) {
                        int32_t index = *((int32_t *) extensionInstance->data);
                        auto p = ext->get_parameter(ext, plugin, index);
                        memcpy(extensionInstance->data, (const void *) &p, sizeof(p));
                    } else {
                        memset(extensionInstance->data, 0, sizeof(aap_parameter_info_t));
                    }
                    return 0;
                });
                break;
            case OPCODE_PARAMETERS_GET_PROPERTY:
                withParametersExtension<int32_t>(plugin, 0, [=](aap_parameters_extension_t *ext,
                                                                AndroidAudioPlugin* plugin) {
                    int32_t parameterId = *((int32_t *) extensionInstance->data);
                    int32_t propertyId = *((int32_t *) extensionInstance->data + 1);
                    *((double *) extensionInstance->data) = ext != nullptr && ext->get_parameter_property ? ext->get_parameter_property(ext, plugin, parameterId, propertyId) : 0.0;
                    return 0;
                });
                break;
            case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT:
                withParametersExtension<int32_t>(plugin, 0, [=](aap_parameters_extension_t *ext,
                                                                AndroidAudioPlugin* plugin) {
                    int32_t parameterId = *((int32_t *) extensionInstance->data);
                    *((int32_t *) extensionInstance->data) = ext != nullptr && ext->get_enumeration_count ? ext->get_enumeration_count(ext, plugin, parameterId) : 0;
                    return 0;
                });
                break;
            case OPCODE_PARAMETERS_GET_ENUMERATION:
                withParametersExtension<int32_t>(plugin, 0, [=](aap_parameters_extension_t *ext,
                                                                AndroidAudioPlugin* plugin) {
                    if (ext != nullptr && ext->get_enumeration) {
                        int32_t parameterId = *((int32_t *) extensionInstance->data);
                        int32_t enumIndex = *((int32_t *) extensionInstance->data + 1);
                        auto e = ext->get_enumeration(ext, plugin, parameterId, enumIndex);
                        memcpy(extensionInstance->data, (const void *) &e, sizeof(e));
                    } else {
                        memset(extensionInstance->data, 0, sizeof(aap_parameter_enum_t));
                    }
                    return 0;
                });
                break;
        }
    }
};


class ParametersExtensionFeature : public PluginExtensionFeatureImpl {
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;

public:
    ParametersExtensionFeature()
            : PluginExtensionFeatureImpl(AAP_PARAMETERS_EXTENSION_URI, false, PARAMETERS_SHARED_MEMORY_SIZE),
              client(std::make_unique<ParametersPluginClientExtension>()),
              service(std::make_unique<ParametersPluginServiceExtension>()) {
    }

    PluginClientExtensionImplBase* getClient() { return client.get(); }
    PluginServiceExtensionImplBase* getService() { return service.get(); }
};

} // namespace aap



#endif //AAP_CORE_PARAMETERS_SERVICE_H
