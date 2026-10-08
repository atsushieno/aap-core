#ifndef AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
#define AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
//-------------------------------------------------------

#include <mutex>
#include <atomic>
#include <chrono>
#include <array>
#include "../realtime.h"
#include "aap/core/aapxs/standard-extensions.h"
#include "aap/unstable/utility.h"
#include "plugin-host.h"
#include "aap/ext/plugin-info.h"
#include "../aap_midi2_helper.h"
#include "aap/core/AAPXSMidi2RecipientSession.h"
#include "aap/core/AAPXSMidi2InitiatorSession.h"
#include "aap/aapxs.h"
#include "aap/core/aapxs/aapxs-lifecycle.h"
#include "aap/core/aapxs/aapxs-hosting-runtime.h"

#define AAP_CORE_REMOTE_NATIVE_UI_PREFERRED_SIZE 1

#if ANDROID
#include <android/trace.h>
#endif

namespace aap {
    namespace internal { struct InstanceRealtimeState; class ParameterValueCache; }

    class PluginSharedMemoryStore;
    class PluginHost;
    class PluginClient;

/**
 * The common basis for client RemotePluginInstance and service LocalPluginInstance.
 *
 * It manages AAPXS and the audio/MIDI2 buffers (by PluginSharedMemoryStore).
 */
    class PluginInstance {
        AndroidAudioPluginFactory *plugin_factory;

    protected:
        int sample_rate{48000};

        void merge_ump_sequences(aap_port_direction portDirection, void *mergeTmp, int32_t mergeBufSize, void* sequence, int32_t sequenceSize, aap_buffer_t *buffer, PluginInstance* instance);

        aap_host_plugin_info_extension_t host_plugin_info{};
        static aap_plugin_info_t
        get_plugin_info(aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char *pluginId);

        int instance_id{-1};
        std::atomic<PluginInstantiationState> instantiation_state{PLUGIN_INSTANTIATION_STATE_INITIAL};
        bool are_ports_configured{false};
        AndroidAudioPlugin *plugin;
        PluginSharedMemoryStore *shared_memory_store{nullptr};
        const PluginInformation *pluginInfo;
        std::unique_ptr <std::vector<PortInformation>> configured_ports{nullptr};
        // indexed by busListIndex(); derived from the port list by rebuildBusesFromPorts().
        std::array<std::vector<BusInformation>, 4> configured_buses{};
        // port index of the main event bus for each direction, or -1. Read on the processing thread.
        std::array<int32_t, 2> main_event_port_indices{-1, -1};
        std::unique_ptr <std::vector<ParameterInformation>> cached_parameters{nullptr};
        std::atomic<const std::vector<ParameterInformation>*> published_parameters{nullptr};
        std::unique_ptr<internal::ParameterValueCache> parameter_values;
        // for client, it collects event inputs and AAPXS SysEx8 UMPs
        // for service, it collects AAPXS SysEx8 UMPs (can be put multiple async results)
        void* event_midi2_buffer{nullptr};
        void* event_midi2_merge_buffer{nullptr};
        int32_t event_midi2_buffer_size{0};
        int32_t event_midi2_buffer_offset{0};
        std::unique_ptr<internal::InstanceRealtimeState> realtime_state;
        virtual void pollExtensionWorker() {}
        virtual std::chrono::steady_clock::time_point nextExtensionDeadline() { return std::chrono::steady_clock::time_point::max(); }
        void startExtensionWorker();
        void releasePlugin(); // derived destructors call while their host facade/proxies still exist
        void mergeQueuedUmp(aap_port_direction direction, bool output = false);

        PluginInstance(const PluginInformation *pluginInformation,
                       AndroidAudioPluginFactory *loadedPluginFactory,
                       int32_t eventMidi2InputBufferSize);

        virtual AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() = 0;

        // port configuration functions
        void setupPortConfigDefaults();
        void setupPortsViaMetadata();
        // Builds the port list and the buses from a bus layout that the service determined, in the
        // canonical order (audio inputs, audio outputs, event inputs, event outputs). Both client
        // and service use it so that they agree on the port list. Non-RT.
        void setupPortsFromBusLayout(const aap_buses_layout_snapshot_t& layout);
        // Must be called whenever the port list is finalized (non-RT).
        void rebuildBusesFromPorts();
        static size_t busListIndex(aap_bus_kind kind, aap_port_direction direction) {
            return (kind == AAP_BUS_KIND_EVENT ? 2 : 0) + (direction == AAP_PORT_DIRECTION_OUTPUT ? 1 : 0);
        }

    public:
        virtual ~PluginInstance();
        internal::InstanceRealtimeState& getRealtimeState() { return *realtime_state; }
        internal::ParameterValueCache& getParameterValueCache() { return *parameter_values; }
        void pollParameterLayoutRefresh(); // extension worker only
        void stopExtensionWorker();
        void requestExtensionWorkerStop();
        bool isOnExtensionWorkerThread() const;

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
            auto* parameters = published_parameters.load(std::memory_order_acquire);
            return parameters ? parameters->size()
                                     : pluginInfo->getNumDeclaredParameters();
        }

        const ParameterInformation *getParameter(int32_t index) {
            if (index < 0) return nullptr;
            auto* parameters = published_parameters.load(std::memory_order_acquire);
            if (!parameters)
                return index < pluginInfo->getNumDeclaredParameters() ? pluginInfo->getDeclaredParameter(index) : nullptr;
            if (parameters->size() > static_cast<size_t>(index))
                return &(*parameters)[index];
            return nullptr;
        }

        int32_t getNumPorts() {
            return configured_ports != nullptr ? configured_ports->size()
                                               : pluginInfo->getNumDeclaredPorts();
        }

        const PortInformation *getPort(int32_t index) {
            if (!configured_ports)
                return pluginInfo->getDeclaredPort(index);
            if (configured_ports->size() > index)
                return &(*configured_ports)[index];
            else {
                AAP_ASSERT_FALSE;
                return nullptr;
            }
        }

        int32_t getNumBuses(aap_bus_kind kind, aap_port_direction direction) {
            return (int32_t) configured_buses[busListIndex(kind, direction)].size();
        }

        const BusInformation *getBus(aap_bus_kind kind, aap_port_direction direction, int32_t index) {
            auto& list = configured_buses[busListIndex(kind, direction)];
            return 0 <= index && (size_t) index < list.size() ? &list[(size_t) index] : nullptr;
        }

        // True if the port (bus) layout is determined by the plugin's buses extension.
        // Otherwise the legacy port configuration applies.
        virtual bool isBusMode() { return false; }

        // Returns the port index of the main event bus buffer, or -1. It is RT-safe.
        int32_t getMainEventPortIndex(aap_port_direction direction) {
            return main_event_port_indices[direction == AAP_PORT_DIRECTION_OUTPUT ? 1 : 0];
        }

        virtual void prepare(int maximumExpectedSamplesPerBlock, int32_t sampleRate) = 0;

        aap::PluginInstantiationState getInstanceState() { return instantiation_state; }

        void activate();

        void deactivate();

        virtual void process(int32_t frameCount, int32_t timeoutInNanoseconds) = 0;

        virtual void setupAAPXS() = 0;
        virtual xs::StandardExtensions &getStandardExtensions() = 0;

        // Async-call abort registry. Every TypedAAPXS (standard or non-standard) registers itself
        // into this shared-owned registry so a transport-level failure (e.g. Binder service death)
        // can fail its in-flight requests. Returns null where there is no client transport to fail
        // (service side); RemotePluginInstance provides a real one.
        virtual std::shared_ptr<xs::AsyncAbortRegistry> getAsyncAbortRegistry() { return nullptr; }
        virtual void abortAllPendingAAPXS(const std::string& error) {}

        uint32_t getTailTimeInMilliseconds() {
            // TODO: FUTURE - most likely just a matter of plugin property
            return 0;
        }

        // It is used by both local and remote plugin instance
        // (UI events for local, host UI interaction etc. for remote)
        void addEventUmpInput(void* input, int32_t size);
        bool tryAddEventUmpInput(const void* input, int32_t size);

        // Returns a serial request Id for AAPXS SysEx8 that increases every time this function is called.
        static uint32_t aapxsRequestIdSerial();

    protected:
        // AAPXS v2
        static inline uint32_t staticGetNewRequestId(AAPXSInitiatorInstance* instance) {
            return ((PluginInstance*) instance->host_context)->aapxsRequestIdSerial();
        }

        static bool
        aapxsSessionAddEventUmpInput(AAPXSMidi2InitiatorSession *client, void *context,
                                     int32_t messageSize);
    };

    typedef void(*aapxs_host_ipc_sender)(void* context,
                                         const char* uri,
                                         int32_t instanceId,
                                         int32_t opcode,
                                         int32_t requestId,
                                         aapxs_completion_callback callback,
                                         void* callbackData,
                                         void* callbackPluginOrHost,
                                         aapxs_error_callback errorCallback);

/**
 * A plugin instance that could use dlopen() and dlsym().
 * FIXME: It should become usable either as a client or a service, but so far it only works as a service.
 */
    class LocalPluginInstance : public PluginInstance {
        PluginHost *host;
        AAPXSMidi2InitiatorSession aapxs_host_session;
        AndroidAudioPluginHost plugin_host_facade{};
        std::unique_ptr<xs::AAPXSDefinitionServiceRegistry> feature_registry;
        xs::AAPXSServiceDispatcher aapxs_dispatcher;
        std::array<void*, 256> host_extension_proxies{};
        std::atomic<bool> process_requested_to_host{false};
        // The layout last reported to the client; confirmPorts() configures the ports from it.
        std::unique_ptr<aap_buses_layout_snapshot_t> reported_bus_layout{};
        std::atomic<uint32_t> bus_layout_generation{1};
        // Bus mode: where the port buffers live in the shared memory pool.
        std::unique_ptr<aap_buffer_layout_t> committed_buffer_layout{};

        class BusesService : public xs::BusesServiceHandler {
            LocalPluginInstance* owner;
        public:
            explicit BusesService(LocalPluginInstance* owner) : owner(owner) {}
            void getBusLayoutSnapshot(aap_buses_layout_snapshot_t& snapshot) override;
            bool commitBufferLayout(const aap_buffer_layout_t& layout) override;
            bool applyLayout(const aap_bus_layout_request_t& request) override;
        };
        BusesService buses_service{this};
        // Its existence tells the plugin that aap_buffer_t has the bus accessors. Notifications go to
        // the host via buses_host_proxy (cached at setupAAPXSInstances(); RT-safe).
        static void notifyBusesChanged(aap_buses_host_extension_t* ext, AndroidAudioPluginHost* host, uint32_t flags);
        aap_buses_host_extension_t host_buses{this, notifyBusesChanged};
        aap_buses_host_extension_t* buses_host_proxy{nullptr};

        AAPXSMidi2RecipientSession aapxs_midi2_in_session{};

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
        void pollExtensionWorker() override;

    public:
        LocalPluginInstance(PluginHost *host,
                            xs::AAPXSDefinitionRegistry *aapxsRegistry,
                            int32_t instanceId,
                            const PluginInformation *pluginInformation,
                            AndroidAudioPluginFactory *loadedPluginFactory,
                            int32_t eventMidi2InputBufferSize);
        virtual ~LocalPluginInstance();

        int32_t getInstanceId() override { return instance_id; }

        void confirmPorts();

        // Invoked by AudioPluginInterfaceImpl::beginPrepare(). A prepared (inactive) instance can be
        // prepared again only in bus mode, with a new buffer pool. Returns an error, or empty.
        std::string beginPrepare();
        // Invoked by AudioPluginInterfaceImpl::endPrepare(), before prepare(). Returns an error, or empty.
        std::string setupPortBuffers(int32_t frameCount);
        // Bus mode: the port buffers live in one shared memory pool.
        bool usesBufferPool() const { return committed_buffer_layout != nullptr; }

        // The client asked for the layout; an older client never does.
        bool isBusMode() override { return reported_bus_layout != nullptr; }
        // Answers the framework-level buses AAPXS requests.
        xs::BusesServiceHandler* getBusesServiceHandler() { return &buses_service; }

        inline AndroidAudioPlugin *getPlugin() { return plugin; }

        std::unique_ptr<aap::xs::ServiceStandardExtensions> standards{nullptr};
        xs::ServiceStandardExtensions &getStandardExtensions() override { return *standards; }
        void setupAAPXS() override;
        void refreshExtensionState(); // control lifecycle; extension-owned snapshots

        // It is invoked by AudioPluginInterfaceImpl and AAPXSMidi2Processor callback,
        // and supposed to dispatch request to extension service
        void controlExtension(uint8_t urid, const std::string &uri, int32_t opcode, uint32_t requestId);

        void prepare(int32_t maximumExpectedSamplesPerBlock, int32_t sampleRate) override;

        void addEventUmpOutput(void* input, int32_t size);
        void process(int32_t frameCount, int32_t timeoutInNanoseconds) override;

        void requestProcessToHost();


        // AAPXS v2
        xs::AAPXSDefinitionServiceRegistry* getAAPXSRegistry() { return feature_registry.get(); }
        xs::AAPXSServiceDispatcher& getAAPXSDispatcher() { return aapxs_dispatcher; }
        bool setupAAPXSInstances();
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

    typedef bool(*aapxs_client_ipc_sender)(void* context,
                          const char* uri,
                          int32_t instanceId,
                          int32_t messageSize,
                          int32_t requestId,
                          int32_t opcode,
                          aapxs_completion_callback callback,
                          void* callbackData,
                          aapxs_error_callback errorCallback);

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
        std::array<void*, 256> plugin_extension_proxies{};

        void* internalGetHostExtension(uint8_t urid, const char *uri);

        static void* staticGetHostExtension(AndroidAudioPluginHost* host, const char* uri) {
            return ((RemotePluginInstance*) host->context)->internalGetHostExtension(0, uri);
        }

        /** it is an unwanted exposure, but we need this internal-only member as public. You are not supposed to use it. */
        aapxs_client_ipc_sender ipc_send_extension_message_impl;

    protected:
        AndroidAudioPluginHost *getHostFacadeForCompleteInstantiation() override;
        void pollExtensionWorker() override;
        std::chrono::steady_clock::time_point nextExtensionDeadline() override;

    public:
        // The `instantiate()` member of the plugin factory is supposed to invoke `setupAAPXSInstances()`.
        // (binder-client-as-plugin does so, and desktop implementation should do so too.)
        RemotePluginInstance(PluginClient* client,
                             xs::AAPXSDefinitionRegistry *aapxsRegistry,
                             const PluginInformation *pluginInformation,
                             AndroidAudioPluginFactory *loadedPluginFactory,
                             int32_t eventMidi2InputBufferSize);
        ~RemotePluginInstance() override;

        int32_t getInstanceId() override {
            // Make sure that we never try to retrieve it before being initialized at completeInstantiation() (at client)
            if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INITIAL) {
                AAP_ASSERT_FALSE;
                return -1;
            }
            return instance_id;
        }

        void setInstanceId(int32_t instanceId) { instance_id = instanceId; }

        void setupUrids();

        // It is performed after endCreate() and beginPrepare(), to configure ports using relevant AAP extensions.
        void configurePorts();

        // Asks the plugin to change its bus layout (only when it is not active). On success, the
        // ports and buses are updated, and the instance becomes UNPREPARED: call prepare() again
        // (with new buffers) before activate(). Returns an error, or empty.
        std::string applyBusLayout(const aap_bus_layout_request_t& request);

        // The plugin declares the buses extension in its metadata.
        bool isBusMode() override { return pluginInfo->hasExtension(AAP_BUSES_EXTENSION_URI); }
    private:
        // The generation of the plugin-provided bus layout, or 0.
        uint32_t bus_layout_generation{0};
        std::mutex buses_changed_handler_mutex{};
        std::function<void(uint32_t flags)> buses_changed_handler{};
        // Changes notified while no handler was set, delivered to the next handler.
        uint32_t pending_buses_changes{0};
        std::string reloadBusLayout(xs::BusesClientAAPXS* buses);
    public:
        // Invoked on the extension worker when the plugin changed its bus names or layout
        // (AAP_BUSES_CHANGED_*). The host calls refreshBusLayout() and prepare() when it is not active.
        // Changes notified before a handler is set are delivered to it on the calling thread.
        void setBusesChangedHandler(std::function<void(uint32_t flags)> handler);
        void dispatchBusesChanged(uint32_t flags);
        // Re-reads the bus layout (when not active). The instance becomes UNPREPARED. Returns an error, or empty.
        std::string refreshBusLayout();


        inline AndroidAudioPlugin *getPlugin() { return plugin; }

        void prepare(int frameCount, int32_t sampleRate) override;
        // `controlBytesPerBlock` is the buffer size of each MIDI2 port.
        void prepare(int frameCount, int32_t sampleRate, int32_t controlBytesPerBlock);

        void process(int32_t frameCount, int32_t timeoutInNanoseconds) override;

        RemotePluginNativeUIController* getNativeUIController() { return native_ui_controller.get(); }

        void* getRemoteWebView();

        // Client has to make sure to instantiate SurfaceView, which is done at this call.
        void prepareSurfaceControlForRemoteNativeUI();
        // Client will have to call this to just return the SurfaceView, to attach to current window (either directly or indirectly).
        // (And this will have to be provided independently of initialization anyways, so it is a separate function).
        void* getRemoteNativeView();
        // Returns the plugin view's preferred Android pixel size before host window decoration.
        bool getRemoteNativeViewPreferredSize(int32_t& width, int32_t& height);
        // Then lastly, client connects to the SurfaceControlViewHost using binder.
        // Note that the SurfaceView needs to have a valid display ID by the time.
        // These steps will have to be done separately, because each of them will involve UI loop.
        void connectRemoteNativeView(int32_t width, int32_t height);
        void configureRemoteNativeView(
            int32_t viewportWidth,
            int32_t viewportHeight,
            int32_t contentWidth,
            int32_t contentHeight,
            int32_t scrollX,
            int32_t scrollY);


        // AAPXS v2
    private:
        std::unique_ptr<xs::AAPXSDefinitionClientRegistry> feature_registry;
        xs::AAPXSClientDispatcher aapxs_dispatcher;
        std::unique_ptr<aap::xs::ClientStandardExtensions> standards{nullptr};
        // Shared-owned so it outlives any TypedAAPXS regardless of member-destruction order.
        std::shared_ptr<xs::AsyncAbortRegistry> async_abort_registry{std::make_shared<xs::AsyncAbortRegistry>()};
    public:

        void setIpcExtensionMessageSender(aapxs_client_ipc_sender sender) {
            ipc_send_extension_message_impl = sender;
        }

        std::shared_ptr<xs::AsyncAbortRegistry> getAsyncAbortRegistry() override { return async_abort_registry; }
        // Fails every in-flight async AAPXS request across all extensions (e.g. on service death).
        void abortAllPendingAAPXS(const std::string& error) override;

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

        // Invoked on the extension worker after a complete refreshed metadata cache is published.
        // Hosts can update their own models from that cache; no additional scan is required.
        std::function<void(RemotePluginInstance& instance)> parametersChangedHandler;

        void setupStandardExtensions();
        void* getPluginExtensionProxy(const char* uri) {
            if (!uri) return nullptr;
            auto urid = feature_registry->items()->getUridMapping()->getUrid(uri);
            return plugin_extension_proxies[urid];
        }
    };
}

#endif //AAP_CORE_AUDIO_PLUGIN_INSTANCE_H
