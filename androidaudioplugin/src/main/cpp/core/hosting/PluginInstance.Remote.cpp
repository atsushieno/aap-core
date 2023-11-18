#include "aap/core/host/plugin-instance.h"
#include "aap/core/host/shared-memory-store.h"
#include "../AAPJniFacade.h"

#define LOG_TAG "AAP.Remote.Instance"

aap::RemotePluginInstance::RemotePluginInstance(PluginClient* client,
                                                xs::AAPXSDefinitionRegistry *aapxsRegistry,
                                                const PluginInformation* pluginInformation,
                                                AndroidAudioPluginFactory* loadedPluginFactory,
                                                int32_t sampleRate,
                                                int32_t eventMidi2InputBufferSize)
        : PluginInstance(pluginInformation, loadedPluginFactory, sampleRate, eventMidi2InputBufferSize),
          client(client),
          aapxs_session(eventMidi2InputBufferSize),
          feature_registry(new xs::AAPXSDefinitionClientRegistry(aapxsRegistry)),
          aapxs_dispatcher(aapxsRegistry)
          {
    shared_memory_store = new ClientPluginSharedMemoryStore();

    aapxs_session.setReplyHandler([&](aap_midi2_aapxs_parse_context* context) {
        handleAAPXSReply(context);
    });
}

void aap::RemotePluginInstance::configurePorts() {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to configurePorts() at state: %d (instanceId: %d)",
                     instantiation_state, instance_id);
        return;
    }

    startPortConfiguration();

    auto ext = plugin->get_extension(plugin, AAP_PORT_CONFIG_EXTENSION_URI);
    if (ext != nullptr) {
        // configure ports using port-config extension.

        // FIXME: implement
        assert(false);

    } else if (pluginInfo->getNumDeclaredPorts() == 0)
        setupPortConfigDefaults();
    else
        setupPortsViaMetadata();
}


AndroidAudioPluginHost* aap::RemotePluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = RemotePluginInstance::staticGetHostExtension;
    return &plugin_host_facade;
}

void aap::RemotePluginInstance::prepare(int frameCount) {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to prepare() at state: %d (instanceId: %d)",
                     instantiation_state, instance_id);
        return;
    }

    auto numPorts = getNumPorts();
    auto shm = dynamic_cast<aap::ClientPluginSharedMemoryStore*>(getSharedMemoryStore());
    auto code = shm->allocateClientBuffer(numPorts, frameCount, *this, DEFAULT_CONTROL_BUFFER_SIZE);
    if (code != aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS) {
        aap::a_log(AAP_LOG_LEVEL_ERROR, LOG_TAG, aap::PluginSharedMemoryStore::getMemoryAllocationErrorMessage(code));
    }

    plugin->prepare(plugin, getAudioPluginBuffer());
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
}

void* aap::RemotePluginInstance::getRemoteWebView()  {
#if ANDROID
    return aap::AAPJniFacade::getInstance()->getRemoteWebView(client, this);
#else
    // cannot do anything
    return nullptr;
#endif
}

void aap::RemotePluginInstance::prepareSurfaceControlForRemoteNativeUI() {
#if ANDROID
    if (!native_ui_controller)
		native_ui_controller = std::make_unique<RemotePluginNativeUIController>(this);
#else
    // cannot do anything
#endif
}

void* aap::RemotePluginInstance::getRemoteNativeView()  {
#if ANDROID
    assert(native_ui_controller);
	return AAPJniFacade::getInstance()->getRemoteNativeView(client, this);
#else
    // cannot do anything
    return nullptr;
#endif
}

void aap::RemotePluginInstance::connectRemoteNativeView(int32_t width, int32_t height)  {
#if ANDROID
    assert(native_ui_controller);
	AAPJniFacade::getInstance()->connectRemoteNativeView(client, this, width, height);
#else
    // cannot do anything
#endif
}

void aap::RemotePluginInstance::process(int32_t frameCount, int32_t timeoutInNanoseconds) {
    const char* remote_trace_name = "AAP::RemotePluginInstance_process";
    struct timespec timeSpecBegin{}, timeSpecEnd{};
#if ANDROID
    if (ATrace_isEnabled()) {
        ATrace_beginSection(remote_trace_name);
        clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
    }
#endif

    // merge input from AAPXS SysEx8 into the host's MIDI inputs
    if (std::unique_lock<NanoSleepLock> tryLock(ump_sequence_merger_mutex, std::try_to_lock); tryLock.owns_lock()) {
        merge_ump_sequences(AAP_PORT_DIRECTION_INPUT, event_midi2_merge_buffer, event_midi2_buffer_size,
                            event_midi2_buffer, event_midi2_buffer_offset,
                            getAudioPluginBuffer(), this);
        event_midi2_buffer_offset = 0;
    }

    // now we can pass the input to the plugin.
    plugin->process(plugin, getAudioPluginBuffer(), frameCount, timeoutInNanoseconds);

    // retrieve AAPXS SysEx8 replies if any.
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_OUTPUT)
            continue;
        auto aapBuffer = getAudioPluginBuffer();
        void* data = aapBuffer->get_buffer(*aapBuffer, i);
        // MIDI2 output buffer has to be processed by this `processReply()` in realtime manner.
        aapxs_session.completeSession(data, plugin);
        ((AAPMidiBufferHeader*) data)->length = 0;
    }

#if ANDROID
    if (ATrace_isEnabled()) {
        clock_gettime(CLOCK_REALTIME, &timeSpecEnd);
        ATrace_setCounter(remote_trace_name,
                          (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 + timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec);
        ATrace_endSection();
    }
#endif
}

void *
aap::RemotePluginInstance::internalGetHostExtension(uint8_t urid, const char *uri) {
    if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
        host_plugin_info.get = get_plugin_info;
        return &host_plugin_info;
    }
    // look for user-implemented host extensions
    if (getHostExtension)
        return getHostExtension(this, urid, uri);
    return nullptr;
}

//----

aap::RemotePluginInstance::RemotePluginNativeUIController::RemotePluginNativeUIController(RemotePluginInstance* owner) {
    handle = AAPJniFacade::getInstance()->createSurfaceControl();
}

aap::RemotePluginInstance::RemotePluginNativeUIController::~RemotePluginNativeUIController() {
    AAPJniFacade::getInstance()->disposeSurfaceControl(handle);
}

void aap::RemotePluginInstance::RemotePluginNativeUIController::show() {
    AAPJniFacade::getInstance()->showSurfaceControlView(handle);
}

void aap::RemotePluginInstance::RemotePluginNativeUIController::hide() {
    AAPJniFacade::getInstance()->hideSurfaceControlView(handle);
}


// ---- AAPXS v2
static inline bool staticSendAAPXSRequest(AAPXSInitiatorInstance* instance, AAPXSRequestContext* context) {
    return ((aap::RemotePluginInstance *) instance->host_context)->sendPluginAAPXSRequest(context);
}
static inline void staticSendAAPXSReply(AAPXSRecipientInstance* instance, AAPXSRequestContext* context) {
    ((aap::RemotePluginInstance*) instance->host_context)->sendHostAAPXSReply(context);
}

bool aap::RemotePluginInstance::setupAAPXSInstances(std::function<bool(const char*, AAPXSSerializationContext*)> sharedMemoryAllocatingRequester) {
    if (!aapxs_dispatcher.setupInstances(this,
                                           sharedMemoryAllocatingRequester,
                                           staticSendAAPXSRequest,
                                           staticSendAAPXSReply,
                                           staticGetNewRequestId))
        return false;
    standards->initialize(&aapxs_dispatcher);
    return true;
}

bool
aap::RemotePluginInstance::sendPluginAAPXSRequest(uint8_t urid, const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId) {
    auto& dispatcher = getAAPXSDispatcher();
    auto aapxsInstance = urid != 0 ? dispatcher.getPluginAAPXSByUrid(urid) : dispatcher.getPluginAAPXSByUri(uri);
    auto serialization = aapxsInstance->serialization;
    memcpy(serialization->data, data, dataSize);
    serialization->data_size = dataSize;
    AAPXSRequestContext request{nullptr, nullptr, serialization, urid, uri, newRequestId, opcode};
    return sendPluginAAPXSRequest(&request);
}

bool
aap::RemotePluginInstance::sendPluginAAPXSRequest(AAPXSRequestContext* request) {
    // If it is at ACTIVE state it has to switch to AAPXS SysEx8 MIDI messaging mode,
    // otherwise it goes to the Binder route.
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // request->serialization already contains binary data here, so we retrieve data from there.
        // This is an asynchronous function, so we do not wait for the result, and it has no awaiter (hence std::nullopt)
        aapxs_session.addSession(aapxsSessionAddEventUmpInput,
                                 this, request);
        return true;
    } else {
        // Here we have to get a native plugin instance and send extension message.
        // It is kind af annoying because we used to implement Binder-specific part only within the
        // plugin API (binder-client-as-plugin.cpp)...
        // So far, instead of rewriting a lot of code to do so, we let AAPClientContext
        // assign its implementation details that handle Binder messaging as a std::function.
        ipc_send_extension_message_impl(plugin->plugin_specific, request->uri, getInstanceId(), request->serialization->data_size, request->opcode);
        return false;
    }
}

void
aap::RemotePluginInstance::processPluginAAPXSReply(AAPXSRequestContext* request) {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // FIXME: should we handle callback here?
        //if (request->callback)
        //    request->callback(request->callback_user_data, plugin, request->request_id);
    } else {
        // nothing to do when non-realtime mode; extension functions are called synchronously.
    }
}

void
aap::RemotePluginInstance::sendHostAAPXSReply(AAPXSRequestContext* request) {
    // If it is at ACTIVE state it has to switch to AAPXS SysEx8 MIDI messaging mode,
    // otherwise it goes to the Binder route.
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // aapxsInstance already contains binary data here, so we retrieve data from there.
        int32_t group = 0; // will we have to give special semantics on it?
        aapxs_session.addSession(aapxsSessionAddEventUmpInput, this, group,
                                 request->request_id, request->urid, request->uri, request->serialization->data,
                                 request->serialization->data_size, request->opcode);
    } else {
        // it was done synchronously, nothing to do here
    }
}

void
aap::RemotePluginInstance::processHostAAPXSRequest(AAPXSRequestContext* context) {
    // FIXME: implement
    throw std::runtime_error("FIXME: implement");
}

aap::xs::AAPXSDefinitionClientRegistry *aap::RemotePluginInstance::getAAPXSRegistry() {
    return feature_registry.get();
}

void aap::RemotePluginInstance::setupAAPXS() {
    standards = std::make_unique<xs::ClientStandardExtensions>();
}

void aap::RemotePluginInstance::handleAAPXSReply(aap_midi2_aapxs_parse_context *context) {
    auto& dispatcher = getAAPXSDispatcher();
    auto registry = feature_registry->items();
    auto aapxs = context->urid != 0 ? registry->getByUrid(context->urid) : registry->getByUri(context->uri);
    if (aapxs) {
        if (context->opcode >= 0) {
            // plugin AAPXS reply
            auto aapxsInstance = context->urid != 0 ? dispatcher.getPluginAAPXSByUrid(context->urid) : dispatcher.getPluginAAPXSByUri(context->uri);
            AAPXSRequestContext request{nullptr, nullptr, aapxsInstance->serialization, context->urid, context->uri, context->request_id, context->opcode};
            if (aapxs->process_incoming_plugin_aapxs_reply)
                aapxs->process_incoming_plugin_aapxs_reply(aapxs, aapxsInstance, plugin, &request);
            else
                aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "AAPXS %s does not have a reply handler (opcode: %d)", context->uri, context->opcode);
        } else {
            // host AAPXS request
            auto aapxsInstance = context->urid != 0 ? dispatcher.getHostAAPXSByUrid(context->urid) : dispatcher.getHostAAPXSByUri(context->uri);
            AAPXSRequestContext request{nullptr, nullptr, aapxsInstance->serialization, context->urid, context->uri, context->request_id, context->opcode};
            if (aapxs->process_incoming_host_aapxs_request)
                aapxs->process_incoming_host_aapxs_request(aapxs, aapxsInstance, &plugin_host_facade, &request);
            else
                aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "AAPXS %s does not have a reply handler (opcode: %d)", context->uri, context->opcode);
        }
    }
    else
        aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "AAPXS for %s is not registered (opcode: %d)", context->uri, context->opcode);
}

void aap::RemotePluginInstance::setupUrids() {
    auto uridExt = (aap_urid_extension_t*) plugin->get_extension(plugin, AAP_URID_EXTENSION_URI);
    if (!uridExt)
        return; // we cannot use URID, bear with any relevant potential latency!

    auto mapping = getAAPXSRegistry()->items()->getUridMapping();
    for (uint8_t urid : *mapping)
        if (urid != 0)
            uridExt->map(uridExt, plugin, urid, mapping->getUri(urid));
}

void aap::RemotePluginInstance::setupStandardExtensions() {
    setupUrids(); // must be done before initializing AAPXSTypedClients.
    standards->initialize(&aapxs_dispatcher);
}
