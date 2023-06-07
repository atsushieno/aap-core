#ifndef AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
#define AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
//-------------------------------------------------------

#include <mutex>
#include "aap/core/aapxs/standard-extensions.h"
#include "plugin-host.h"

#if ANDROID
#include <android/trace.h>
#endif

namespace aap {

    class PluginSharedMemoryStore;
    class PluginHost;
    class PluginClient;

/**
 * The common basis for client RemotePluginInstance and service LocalPluginInstance.
 *
 * It manages AAPXS and the audio/MIDI2 buffers (by PluginSharedMemoryStore).
 */
    class PluginInstance {
        int sample_rate{44100};

        AndroidAudioPluginFactory *plugin_factory;

        // FIXME: unify code with androidaudioplugin-midi-device-service
        class NanoSleepLock {
            std::atomic_flag state = ATOMIC_FLAG_INIT;
        public:
            void lock() noexcept {
                const auto delay = timespec{0, 1000}; // 1 microsecond
                while(state.test_and_set())
                    clock_nanosleep(CLOCK_REALTIME, 0, &delay, nullptr);
            }
            void unlock() noexcept { state.clear(); }
            bool try_lock() noexcept { return !state.test_and_set(); }
        };
        NanoSleepLock event_input_buffer_mutex{};

    protected:
        int instance_id{-1};
        PluginInstantiationState instantiation_state{PLUGIN_INSTANTIATION_STATE_INITIAL};
        bool are_ports_configured{false};
        AndroidAudioPlugin *plugin;
        PluginSharedMemoryStore *shared_memory_store{nullptr};
        const PluginInformation *pluginInfo;
        std::unique_ptr <std::vector<PortInformation>> configured_ports{nullptr};
        std::unique_ptr <std::vector<ParameterInformation>> cached_parameters{nullptr};
        void* event_midi2_input_buffer{nullptr};
        void* event_midi2_input_buffer_merged{nullptr};
        int32_t event_midi2_input_buffer_size{0};
        int32_t event_midi2_input_buffer_offset{0};

        PluginInstance(const PluginInformation *pluginInformation,
                       AndroidAudioPluginFactory *loadedPluginFactory,
                       int32_t sampleRate,
                       int32_t eventMidi2InputBufferSize);

        virtual AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() = 0;

    public:

        virtual ~PluginInstance();

        virtual int32_t getInstanceId() = 0;

        inline PluginSharedMemoryStore *getSharedMemoryStore() { return shared_memory_store; }

        // It may or may not be shared memory buffer.
        aap_buffer_t *getAudioPluginBuffer();

        const PluginInformation *getPluginInformation() {
            return pluginInfo;
        }

        void completeInstantiation();

        // common to both service and client.
        void startPortConfiguration() {
            configured_ports = std::make_unique < std::vector < PortInformation >> ();

            /* FIXME: enable this once we fix configurePorts() for service.
            // Add mandatory system common ports
            PortInformation core_midi_in{-1, "System Common Host-To-Plugin", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
            PortInformation core_midi_out{-2, "System Common Plugin-To-Host", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_OUTPUT};
            PortInformation core_midi_rt{-3, "System Realtime (HtP)", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
            configured_ports->emplace_back(core_midi_in);
            configured_ports->emplace_back(core_midi_out);
            configured_ports->emplace_back(core_midi_rt);
            */
        }

        void setupPortConfigDefaults();

        // This means that there was no configured ports by extensions.
        void setupPortsViaMetadata() {
            if (are_ports_configured)
                return;
            are_ports_configured = true;

            for (int i = 0, n = pluginInfo->getNumDeclaredPorts(); i < n; i++)
                configured_ports->emplace_back(PortInformation{*pluginInfo->getDeclaredPort(i)});
        }

        void scanParametersAndBuildList();

        int32_t getNumParameters() {
            return cached_parameters ? cached_parameters->size()
                                     : pluginInfo->getNumDeclaredParameters();
        }

        const ParameterInformation *getParameter(int32_t index) {
            if (!cached_parameters)
                return pluginInfo->getDeclaredParameter(index);
            assert(cached_parameters->size() > index);
            return &(*cached_parameters)[index];
        }

        int32_t getNumPorts() {
            return configured_ports != nullptr ? configured_ports->size()
                                               : pluginInfo->getNumDeclaredPorts();
        }

        const PortInformation *getPort(int32_t index) {
            if (!configured_ports)
                return pluginInfo->getDeclaredPort(index);
            assert(configured_ports->size() > index);
            return &(*configured_ports)[index];
        }

        virtual void prepare(int maximumExpectedSamplesPerBlock) = 0;

        void activate() {
            if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
                return;
            assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);

            plugin->activate(plugin);
            instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
        }

        void deactivate() {
            if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE)
                return;
            assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);

            plugin->deactivate(plugin);
            instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
        }

        const char* remote_trace_name = "AAP::RemotePluginInstance_process";
        const char* local_trace_name = "AAP::LocalPluginInstance_process";

        virtual void process(int32_t frameCount, int32_t timeoutInNanoseconds);

        virtual StandardExtensions &getStandardExtensions() = 0;

        uint32_t getTailTimeInMilliseconds() {
            // TODO: FUTURE - most likely just a matter of plugin property
            return 0;
        }

        // Event controller (GUI) support
        void addEventUmpInput(void* input, int32_t size);
    };

/**
 * A plugin instance that could use dlopen() and dlsym(). It can be either client side or host side.
 */
    class LocalPluginInstance : public PluginInstance {
        class LocalPluginInstanceStandardExtensionsImpl
                : public LocalPluginInstanceStandardExtensions {
            LocalPluginInstance *owner;

        public:
            LocalPluginInstanceStandardExtensionsImpl(LocalPluginInstance *owner)
                    : owner(owner) {
            }

            AndroidAudioPlugin *getPlugin() override { return owner->getPlugin(); }
        };

        PluginHost *host;
        AAPXSRegistry *aapxs_registry;
        AndroidAudioPluginHost plugin_host_facade{};
        AAPXSInstanceMap <AAPXSServiceInstance> aapxsServiceInstances;
        LocalPluginInstanceStandardExtensionsImpl standards;
        aap_host_plugin_info_extension_t host_plugin_info{};
        aap_host_parameters_extension_t host_parameters_extension{};
        bool process_requested{false};

        static aap_plugin_info_t
        get_plugin_info(aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char *pluginId);

        static void notify_parameters_changed(aap_host_parameters_extension_t* ext, AndroidAudioPluginHost *host, AndroidAudioPlugin *plugin);

        inline static void *
        internalGetExtension(AndroidAudioPluginHost *host, const char *uri) {
            if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
                auto instance = (LocalPluginInstance *) host->context;
                instance->host_plugin_info.get = get_plugin_info;
                return &instance->host_plugin_info;
            }
            if (strcmp(uri, AAP_PARAMETERS_EXTENSION_URI) == 0) {
                auto instance = (LocalPluginInstance *) host->context;
                instance->host_parameters_extension.notify_parameters_changed = notify_parameters_changed;
                return &instance->host_parameters_extension;
            }
            return nullptr;
        }

        inline static void internalRequestProcess(AndroidAudioPluginHost *host) {
            auto instance = (LocalPluginInstance *) host->context;
            instance->requestProcessToHost();
        }

    protected:
        AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() override;

    public:
        LocalPluginInstance(PluginHost *host, AAPXSRegistry *aapxsRegistry, int32_t instanceId,
                            const PluginInformation *pluginInformation,
                            AndroidAudioPluginFactory *loadedPluginFactory, int32_t sampleRate,
                            int32_t eventMidi2InputBufferSize);

        int32_t getInstanceId() override { return instance_id; }

        void confirmPorts() {
            // FIXME: implementation is feature parity with client side so far, but it should be based on port config negotiation.
            auto ext = plugin->get_extension(plugin, AAP_PORT_CONFIG_EXTENSION_URI);
            if (ext != nullptr) {
                // configure ports using port-config extension.

                // FIXME: implement

            } else if (pluginInfo->getNumDeclaredPorts() == 0)
                setupPortConfigDefaults();
            else
                setupPortsViaMetadata();
        }

        inline AndroidAudioPlugin *getPlugin() { return plugin; }

        // unlike client host side, this function is invoked for each `addExtension()` Binder call,
        // which is way simpler.
        AAPXSServiceInstance *setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize = -1) {
            const char *uri = aapxsServiceInstances.addOrGetUri(feature->uri);
            assert(aapxsServiceInstances.get(uri) == nullptr);
            if (dataSize < 0)
                dataSize = feature->shared_memory_size;
            aapxsServiceInstances.add(uri, std::make_unique<AAPXSServiceInstance>(
                    AAPXSServiceInstance{this, uri, getInstanceId(), nullptr, dataSize}));
            return aapxsServiceInstances.get(uri);
        }

        AAPXSServiceInstance *getInstanceFor(const char *uri) {
            auto ret = aapxsServiceInstances.get(uri);
            assert(ret);
            return ret;
        }

        StandardExtensions &getStandardExtensions() override { return standards; }

        // It is invoked by AudioPluginInterfaceImpl, and supposed to dispatch request to extension service
        void controlExtension(const std::string &uri, int32_t opcode) {
            auto aapxsInstance = getInstanceFor(uri.c_str());
            auto feature = aapxs_registry->getByUri(uri.c_str());
            feature->on_invoked(feature, getPlugin(), aapxsInstance, opcode);
        }

        void prepare(int32_t maximumExpectedSamplesPerBlock) override {
            assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED ||
                   instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);

            plugin->prepare(plugin, getAudioPluginBuffer());
            instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
        }

        void process(int32_t frameCount, int32_t timeoutInNanoseconds) override {
            process_requested = false;
            PluginInstance::process(frameCount, timeoutInNanoseconds);
        }

        void requestProcessToHost();
    };

    class RemotePluginInstance : public PluginInstance {
        class RemotePluginInstanceStandardExtensionsImpl
                : public RemotePluginInstanceStandardExtensions {
            RemotePluginInstance *owner;

        public:
            RemotePluginInstanceStandardExtensionsImpl(RemotePluginInstance *owner)
                    : owner(owner) {
            }

            AndroidAudioPlugin* getPlugin() override { return owner->getPlugin(); }

            AAPXSClientInstanceManager* getAAPXSManager() override { return owner->getAAPXSManager(); }
        };

        // holds AudioPluginSurfaceControlClient for plugin instance lifetime.
        class RemotePluginNativeUIController {
            void* handle;
        public:
            RemotePluginNativeUIController(RemotePluginInstance* owner);
            virtual ~RemotePluginNativeUIController();

            void* getHandle() { return handle; }
            void* getView();
            void show();
            void hide();
        };

        PluginClient *client;
        AAPXSRegistry *aapxs_registry;
        AndroidAudioPluginHost plugin_host_facade{};
        RemotePluginInstanceStandardExtensionsImpl standards;
        std::unique_ptr<AAPXSClientInstanceManager> aapxs_manager;
        std::unique_ptr<RemotePluginNativeUIController> native_ui_controller{};

        friend class RemoteAAPXSManager;

    protected:
        AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() override;

    public:
        // The `instantiate()` member of the plugin factory is supposed to invoke `setupAAPXSInstances()`.
        // (binder-client-as-plugin does so, and desktop implementation should do so too.)
        RemotePluginInstance(PluginClient* client,
                             AAPXSRegistry *aapxsRegistry,
                             const PluginInformation *pluginInformation,
                             AndroidAudioPluginFactory *loadedPluginFactory, int32_t sampleRate,
                             int32_t eventMidi2InputBufferSize);

        int32_t getInstanceId() override {
            // Make sure that we never try to retrieve it before being initialized at completeInstantiation() (at client)
            assert(instantiation_state != PLUGIN_INSTANTIATION_STATE_INITIAL);
            return instance_id;
        }

        void setInstanceId(int32_t instanceId) { instance_id = instanceId; }

        // It is performed after endCreate() and beginPrepare(), to configure ports using relevant AAP extensions.
        void configurePorts();

        /** it is an unwanted exposure, but we need this internal-only member as public. You are not supposed to use it. */
        std::function<void(const char *, int32_t, int32_t)> send_extension_message_impl;

        inline AndroidAudioPlugin *getPlugin() { return plugin; }

        AAPXSClientInstanceManager *getAAPXSManager() { return aapxs_manager.get(); }

        void sendExtensionMessage(const char *uri, int32_t opcode);

        StandardExtensions &getStandardExtensions() override { return standards; }

        void prepare(int frameCount) override;

        [[deprecated("use getRemoteWebView()")]]
        void* getWebView() { return getRemoteWebView(); }

        RemotePluginNativeUIController* getNativeUIController() { return native_ui_controller.get(); }

        void* getRemoteWebView();

        // Client has to make sure to instantiate SurfaceView, which is done at this call.
        void prepareSurfaceControlForRemoteNativeUI();
        // Client will have to call this to just return the SurfaceView, to attach to current window (either directly or indirectly).
        // (And this will have to be provided independently of initialization anyways, so it is a separate function).
        void* getRemoteNativeView();
        // Then lastly, client connects to the SurfaceControlViewHost using binder.
        // Note that the SurfaceView needs to have a valid display ID by the time.
        // These steps will have to be done separately, because each of them will involve UI loop.
        void connectRemoteNativeView(int32_t width, int32_t height);
    };

    // The AAPXSClientInstanceManager implementation specific to RemotePluginInstance
    class RemoteAAPXSManager : public AAPXSClientInstanceManager {
        RemotePluginInstance *owner;

    protected:

        static void staticSendExtensionMessage(AAPXSClientInstance *clientInstance, int32_t opcode);

    public:
        RemoteAAPXSManager(RemotePluginInstance *owner)
                : owner(owner) {
        }

        const PluginInformation *
        getPluginInformation() override { return owner->getPluginInformation(); }

        AndroidAudioPlugin *getPlugin() override { return owner->plugin; }

        AAPXSFeature *getExtensionFeature(const char *uri) override {
            return owner->aapxs_registry->getByUri(uri);
        }

        AAPXSClientInstance *setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) override;
    };

}

#endif //AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
