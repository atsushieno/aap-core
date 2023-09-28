#ifndef AAP_CORE_PLUGIN_HOST_H
#define AAP_CORE_PLUGIN_HOST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <memory>
#include <vector>
#include <map>
#include <string>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/logging.h"
#include "aap/ext/parameters.h"
#include "aap/ext/presets.h"
#include "aap/ext/state.h"
#include "aap/ext/port-config.h"
#include "aap/ext/plugin-info.h"
#include "plugin-connections.h"
#include "plugin-instance.h"
#include "../aapxs/extension-service.h"
#include "../aapxs/standard-extensions.h"

namespace aap {

    class PluginInstance;
    class LocalPluginInstance;

    class AudioPluginServiceCallback {
    public:
        virtual ~AudioPluginServiceCallback() {}

        virtual void hostExtension(int32_t in_instanceId, const std::string& in_uri, int32_t in_opcode) = 0;
        virtual void requestProcess(int32_t in_instanceId) = 0;
    };

/* Common foundation for both Plugin service and Plugin client to provide common features:
 *
 * - retrieve AAPXSFeature*
 * - manage plugins (instantiate and destroy) within either a client (to multiple service connections)
 *   or a service (to multiple client connections)
 *
 */
    class PluginHost
    {
    protected:
        AAPXSRegistry* aapxs_registry{nullptr};
        PluginListSnapshot* plugin_list{nullptr};

        std::vector<PluginInstance*> instances{};
        PluginInstance* instantiateLocalPlugin(const PluginInformation *pluginInfo, int sampleRate);
        int32_t event_midi2_input_buffer_size{4096};

    public:
        PluginHost(PluginListSnapshot* contextPluginList, AAPXSRegistry* aapxsRegistry = nullptr, int32_t eventMidi2InputBufferSize = 4096);

        virtual ~PluginHost() {}

        AAPXSFeature* getExtensionFeature(const char* uri);

        void destroyInstance(PluginInstance* instance);

        size_t getInstanceCount() { return instances.size(); }

        // Note that the argument is NOT instanceId
        PluginInstance* getInstanceByIndex(int32_t index);

        PluginInstance* getInstanceById(int32_t instanceId);
    };


    class PluginService : public PluginHost {
        AudioPluginServiceCallback* plugin_service_callback;
    public:
        PluginService(PluginListSnapshot* contextPluginList, AudioPluginServiceCallback* callback, AAPXSRegistry* aapxsRegistry = nullptr)
                : PluginHost(contextPluginList, aapxsRegistry), plugin_service_callback(callback)
        {
        }

        int createInstance(std::string identifier, int sampleRate);

        inline LocalPluginInstance* getLocalInstance(int32_t instanceId) {
            return (LocalPluginInstance*) getInstanceById(instanceId);
        }

        void requestProcessToHost(int32_t instanceId) {
            plugin_service_callback->requestProcess(instanceId);
        }
    };

    class PluginClient : public PluginHost {
        PluginClientConnectionList* connections;

        template<typename T>
        struct Result {
            T value;
            std::string error;
        };

        Result<int32_t> instantiateRemotePlugin(const PluginInformation *pluginInfo, int sampleRate);

    public:
        PluginClient(PluginClientConnectionList* pluginConnections, PluginListSnapshot* contextPluginList, AAPXSRegistry* aapxsRegistry = nullptr)
                : PluginHost(contextPluginList, aapxsRegistry), connections(pluginConnections)
        {
        }

        inline PluginClientConnectionList* getConnections() { return connections; }

        // Synchronous version that does not expect service connection on the fly (fails immediately).
        // It is probably better suited for Kotlin client to avoid complicated JNI interop.
        Result<int32_t> createInstance(std::string identifier, int sampleRate, bool isRemoteExplicit);

        void connectToPluginService(const std::string& identifier, std::function<void(std::string&)> callback);

        void connectToPluginService(const std::string& packageName, const std::string& className, std::function<void(std::string&)> callback);

        // Asynchronous version that allows service connection on the fly.
        [[deprecated("Use connectToPluginService() for async connection establishment and then createInstance(), instead of this function.")]]
        void createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string&)>& callback);
    };

} // namespace


#endif //AAP_CORE_PLUGIN_HOST_H
