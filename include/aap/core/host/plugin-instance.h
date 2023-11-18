#ifndef AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
#define AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
//-------------------------------------------------------

#include <mutex>
#include "aap/core/aapxs/standard-extensions.h"
#include "aap/unstable/utility.h"
#include "plugin-host.h"
#include "aap/ext/plugin-info.h"
#include "../aap_midi2_helper.h"
#include "aap/core/AAPXSMidi2RecipientSession.h"
#include "aap/core/AAPXSMidi2InitiatorSession.h"
#include "aap/aapxs.h"
#include "aap/core/aapxs/aapxs-hosting-runtime.h"

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

    protected:
        NanoSleepLock ump_sequence_merger_mutex{};
        void merge_ump_sequences(aap_port_direction portDirection, void *mergeTmp, int32_t mergeBufSize, void* sequence, int32_t sequenceSize, aap_buffer_t *buffer, PluginInstance* instance);

        aap_host_plugin_info_extension_t host_plugin_info{};
        static aap_plugin_info_t
        get_plugin_info(aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char *pluginId);

        int instance_id{-1};
        PluginInstantiationState instantiation_state{PLUGIN_INSTANTIATION_STATE_INITIAL};
        bool are_ports_configured{false};
        AndroidAudioPlugin *plugin;
        PluginSharedMemoryStore *shared_memory_store{nullptr};
        const PluginInformation *pluginInfo;
        std::unique_ptr <std::vector<PortInformation>> configured_ports{nullptr};
        std::unique_ptr <std::vector<ParameterInformation>> cached_parameters{nullptr};
        // for client, it collects event inputs and AAPXS SysEx8 UMPs
        // for service, it collects AAPXS SysEx8 UMPs (can be put multiple async results)
        void* event_midi2_buffer{nullptr};
        void* event_midi2_merge_buffer{nullptr};
        int32_t event_midi2_buffer_size{0};
        int32_t event_midi2_buffer_offset{0};

        PluginInstance(const PluginInformation *pluginInformation,
                       AndroidAudioPluginFactory *loadedPluginFactory,
                       int32_t sampleRate,
                       int32_t eventMidi2InputBufferSize);

        virtual AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() = 0;

        // port configuration functions
        void setupPortConfigDefaults();
        void setupPortsViaMetadata();

    public:
        virtual ~PluginInstance();

        virtual int32_t getInstanceId() = 0;

        PluginSharedMemoryStore *getSharedMemoryStore() { return shared_memory_store; }

        // It may or may not be shared memory buffer.
        aap_buffer_t *getAudioPluginBuffer();

        const PluginInformation *getPluginInformation() { return pluginInfo; }

        void completeInstantiation();

        // common to both service and client.
        void startPortConfiguration();

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

        aap::PluginInstantiationState getInstanceState() { return instantiation_state; }

        void activate();

        void deactivate();

        virtual void process(int32_t frameCount, int32_t timeoutInNanoseconds) = 0;

        virtual void setupAAPXS() = 0;
        virtual xs::StandardExtensions &getStandardExtensions() = 0;

        uint32_t getTailTimeInMilliseconds() {
            // TODO: FUTURE - most likely just a matter of plugin property
            return 0;
        }

        // It is used by both local and remote plugin instance
        // (UI events for local, host UI interaction etc. for remote)
        void addEventUmpInput(void* input, int32_t size);

        // Returns a serial request Id for AAPXS SysEx8 that increases every time this function is called.
        static uint32_t aapxsRequestIdSerial();

    protected:
        // AAPXS v2
        static inline uint32_t staticGetNewRequestId(AAPXSInitiatorInstance* instance) {
            return ((PluginInstance*) instance->host_context)->aapxsRequestIdSerial();
        }

        static void
        aapxsSessionAddEventUmpInput(AAPXSMidi2InitiatorSession *client, void *context,
                                     int32_t messageSize);
    };

    typedef void(*aapxs_host_ipc_sender)(void* context,
                                         const char* uri,
                                         int32_t instanceId,
                                         int32_t opcode);

/**
 * A plugin instance that could use dlopen() and dlsym().
 * FIXME: It should become usable either as a client or a service, but so far it only works as a service.
 */
    class LocalPluginInstance : public PluginInstance {
        PluginHost *host;
        xs::UridMapping urid_mapping{};
        AAPXSMidi2InitiatorSession aapxs_host_session;
        AndroidAudioPluginHost plugin_host_facade{};
        std::unique_ptr<xs::AAPXSDefinitionServiceRegistry> feature_registry;
        xs::AAPXSServiceDispatcher aapxs_dispatcher;
        bool process_requested_to_host{false};

        AAPXSMidi2RecipientSession aapxs_midi2_in_session{};
        NanoSleepLock aapxs_out_merger_mutex_out{};
        void* aapxs_out_midi2_buffer{nullptr};
        void* aapxs_out_merge_buffer{nullptr};
        int32_t aapxs_out_midi2_buffer_offset{0};

        static void* internalGetHostExtension(AndroidAudioPluginHost *host, const char *uri) {
            return ((LocalPluginInstance*) host->context)->getHostExtension(0, uri);
        }
        void* getHostExtension(uint8_t urid, const char *uri);
        static void internalRequestProcess(AndroidAudioPluginHost *host);

        /** it is an unwanted exposure, but we need this internal-only member as public. You are not supposed to use it. */
        aapxs_host_ipc_sender ipc_send_extension_message_func;
        void* ipc_send_extension_message_context;

        void setupUrids();

    protected:
        AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() override;

    public:
        LocalPluginInstance(PluginHost *host,
                            xs::AAPXSDefinitionRegistry *aapxsRegistry,
                            int32_t instanceId,
                            const PluginInformation *pluginInformation,
                            AndroidAudioPluginFactory *loadedPluginFactory, int32_t sampleRate,
                            int32_t eventMidi2InputBufferSize);
        virtual ~LocalPluginInstance();

        int32_t getInstanceId() override { return instance_id; }

        void confirmPorts();

        inline AndroidAudioPlugin *getPlugin() { return plugin; }

        std::unique_ptr<aap::xs::ServiceStandardExtensions> standards{nullptr};
        xs::ServiceStandardExtensions &getStandardExtensions() override { return *standards; }
        void setupAAPXS() override;

        // It is invoked by AudioPluginInterfaceImpl and AAPXSMidi2Processor callback,
        // and supposed to dispatch request to extension service
        void controlExtension(uint8_t urid, const std::string &uri, int32_t opcode, uint32_t requestId);

        void prepare(int32_t maximumExpectedSamplesPerBlock) override {
            assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED ||
                   instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);

            plugin->prepare(plugin, getAudioPluginBuffer());
            instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
        }

        void addEventUmpOutput(void* input, int32_t size);

        void process(int32_t frameCount, int32_t timeoutInNanoseconds) override;

        void requestProcessToHost();


        // AAPXS v2
        xs::AAPXSDefinitionServiceRegistry* getAAPXSRegistry() { return feature_registry.get(); }
        xs::AAPXSServiceDispatcher& getAAPXSDispatcher() { return aapxs_dispatcher; }
        void setupAAPXSInstances();
        void sendPluginAAPXSReply(AAPXSRequestContext* request);
        // returns true if it is asynchronously invoked without waiting for result,
        // or false if it is synchronously completed.
        // Note that, however, there should not be synchronous callback in ACTIVE (realtime) mode, because
        // there will not be any RT-safe return mode across multiple `process()` calls.
        bool sendHostAAPXSRequest(AAPXSRequestContext* request);

        void setIpcExtensionMessageSender(aapxs_host_ipc_sender sender, void* context) {
            ipc_send_extension_message_func = sender;
            ipc_send_extension_message_context = context;
        }

        void handleAAPXSInput(aap_midi2_aapxs_parse_context *context);
    };

    typedef void(*aapxs_client_ipc_sender)(void* context,
                          const char* uri,
                          int32_t instanceId,
                          int32_t messageSize,
                          int32_t opcode);

    class RemotePluginInstance : public PluginInstance {
        // holds AudioPluginSurfaceControlClient for plugin instance lifetime.
        class RemotePluginNativeUIController {
            void* handle;
        public:
            RemotePluginNativeUIController(RemotePluginInstance* owner);
            virtual ~RemotePluginNativeUIController();

            void* getHandle() { return handle; }
            void show();
            void hide();
        };

        PluginClient *client;
        AndroidAudioPluginHost plugin_host_facade{};
        AAPXSMidi2InitiatorSession aapxs_session;
        std::unique_ptr<RemotePluginNativeUIController> native_ui_controller{};

        void* internalGetHostExtension(uint8_t urid, const char *uri);

        static void* staticGetHostExtension(AndroidAudioPluginHost* host, const char* uri) {
            return ((RemotePluginInstance*) host->context)->internalGetHostExtension(0, uri);
        }

        /** it is an unwanted exposure, but we need this internal-only member as public. You are not supposed to use it. */
        aapxs_client_ipc_sender ipc_send_extension_message_impl;

    protected:
        AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() override;

    public:
        // The `instantiate()` member of the plugin factory is supposed to invoke `setupAAPXSInstances()`.
        // (binder-client-as-plugin does so, and desktop implementation should do so too.)
        RemotePluginInstance(PluginClient* client,
                             xs::AAPXSDefinitionRegistry *aapxsRegistry,
                             const PluginInformation *pluginInformation,
                             AndroidAudioPluginFactory *loadedPluginFactory, int32_t sampleRate,
                             int32_t eventMidi2InputBufferSize);

        int32_t getInstanceId() override {
            // Make sure that we never try to retrieve it before being initialized at completeInstantiation() (at client)
            assert(instantiation_state != PLUGIN_INSTANTIATION_STATE_INITIAL);
            return instance_id;
        }

        void setInstanceId(int32_t instanceId) { instance_id = instanceId; }

        void setupUrids();

        // It is performed after endCreate() and beginPrepare(), to configure ports using relevant AAP extensions.
        void configurePorts();

        inline AndroidAudioPlugin *getPlugin() { return plugin; }

        void prepare(int frameCount) override;

        void process(int32_t frameCount, int32_t timeoutInNanoseconds) override;

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


        // AAPXS v2
    private:
        std::unique_ptr<xs::AAPXSDefinitionClientRegistry> feature_registry;
        xs::AAPXSClientDispatcher aapxs_dispatcher;
        std::unique_ptr<aap::xs::ClientStandardExtensions> standards{nullptr};
    public:

        void setIpcExtensionMessageSender(aapxs_client_ipc_sender sender) {
            ipc_send_extension_message_impl = sender;
        }

        void setupAAPXS() override;
        inline xs::AAPXSClientDispatcher& getAAPXSDispatcher() { return aapxs_dispatcher; }
        bool setupAAPXSInstances(std::function<bool(const char*, AAPXSSerializationContext*)> sharedMemoryAllocatingRequester);
        // Intended for invocation from JNI.
        // returns true if it is asynchronously invoked without waiting for result,
        // or false if it is synchronously completed.
        bool sendPluginAAPXSRequest(uint8_t urid, const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId);
        // returns true if it is asynchronously invoked without waiting for result,
        // or false if it is synchronously completed.
        bool sendPluginAAPXSRequest(AAPXSRequestContext* context);
        void processPluginAAPXSReply(AAPXSRequestContext* context);
        void sendHostAAPXSReply(AAPXSRequestContext* context);
        void processHostAAPXSRequest(AAPXSRequestContext* context);

        // AAPXS v2 registry
        xs::AAPXSDefinitionClientRegistry* getAAPXSRegistry();
        xs::StandardExtensions &getStandardExtensions() override { return *standards; }

        void handleAAPXSReply(aap_midi2_aapxs_parse_context *context);

        // Host developers can override this function to return their own extensions.
        std::function<void*(RemotePluginInstance *instance, uint8_t urid, const char *uri)> getHostExtension;

        void setupStandardExtensions();
    };
}

#endif //AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
