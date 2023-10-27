#include "aap/core/host/plugin-instance.h"
#include "aap/core/host/shared-memory-store.h"
#include "../AAPJniFacade.h"

#define LOG_TAG "AAP.Remote.Instance"

aap::RemotePluginInstance::RemotePluginInstance(PluginClient* client,
                                                AAPXSRegistry* aapxsRegistry,
                                                const PluginInformation* pluginInformation,
                                                AndroidAudioPluginFactory* loadedPluginFactory,
                                                int32_t sampleRate,
                                                int32_t eventMidi2InputBufferSize)
        : PluginInstance(pluginInformation, loadedPluginFactory, sampleRate, eventMidi2InputBufferSize),
          client(client),
          aapxs_registry(aapxsRegistry),
          standards(this),
          aapxs_manager(std::make_unique<RemoteAAPXSManager>(this)),
          aapxs_session(eventMidi2InputBufferSize) {
    shared_memory_store = new ClientPluginSharedMemoryStore();

    aapxs_session.setReplyHandler([&](aap_midi2_aapxs_parse_context* context) {
        auto aapxsInstance = aapxs_manager->getInstanceFor(context->uri);
        if (aapxsInstance->handle_extension_reply)
            aapxsInstance->handle_extension_reply(aapxsInstance, context->dataSize, context->opcode, context->request_id);
        else
            aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "AAPXS %s does not have a reply handler (opcode: %d)", context->uri, context->opcode);
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
    plugin_host_facade.get_extension = RemotePluginInstance::internalGetHostExtension;
    return &plugin_host_facade;
}

void aapxsSessionAddEventUmpInput(aap::AAPXSMidi2ClientSession* client, void* context, int32_t messageSize) {
    auto instance = (aap::RemotePluginInstance *) context;
    instance->addEventUmpInput(client->aapxs_rt_midi_buffer, messageSize);
}

void aap::RemotePluginInstance::sendExtensionMessage(const char *uri, int32_t messageSize, int32_t opcode) {
    auto aapxsInstance = aapxs_manager->getInstanceFor(uri);

    // If it is at ACTIVE state it has to switch to AAPXS SysEx8 MIDI messaging mode,
    // otherwise it goes to the Binder route.
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // aapxsInstance already contains binary data here, so we retrieve data from there.
        int32_t group = 0; // will we have to give special semantics on it?
        int32_t requestId = aapxsSysEx8RequestSerial();
        std::promise<int32_t> promise;
        auto future = aapxs_session.addSession(aapxsSessionAddEventUmpInput, this, group, requestId, aapxsInstance, messageSize,  opcode, std::move(promise));
        // This is a synchronous function, so we have to wait for the result...
        future.wait();
    } else {
        // Here we have to get a native plugin instance and send extension message.
        // It is kind af annoying because we used to implement Binder-specific part only within the
        // plugin API (binder-client-as-plugin.cpp)...
        // So far, instead of rewriting a lot of code to do so, we let AAPClientContext
        // assign its implementation details that handle Binder messaging as a std::function.
        ipc_send_extension_message_impl(plugin->plugin_specific, aapxsInstance->uri, getInstanceId(), messageSize, opcode);
    }
}

void aap::RemotePluginInstance::processExtensionReply(const char *uri, int32_t messageSize,
                                                      int32_t opcode, int32_t requestId) {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        aapxs_session.promises[requestId].set_value(0);
        aapxs_session.promises.erase(requestId);
    } else {
        // nothing to do when non-realtime mode; extension functions are called synchronously.
    }
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

uint32_t aapxs_request_id_serial{0};
uint32_t aap::RemotePluginInstance::aapxsSysEx8RequestSerial() {
    return aapxs_request_id_serial++;
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
        aapxs_session.processReply(data);
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
aap::RemotePluginInstance::internalGetHostExtension(AndroidAudioPluginHost *host, const char *uri)  {
    if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
        auto instance = (RemotePluginInstance *) host->context;
        instance->host_plugin_info.get = get_plugin_info;
        return &instance->host_plugin_info;
    }
    return ((RemotePluginInstance*) host->context)->getAAPXSManager()->getExtensionProxy(uri).extension;
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


//----

AAPXSClientInstance* aap::RemoteAAPXSManager::setupAAPXSInstance(AAPXSFeature *feature, int32_t dataCapacity) {
    const char* uri = aapxsClientInstances.addOrGetUri(feature->uri);
    assert (aapxsClientInstances.get(uri) == nullptr);
    if (dataCapacity < 0)
        dataCapacity = feature->shared_memory_size;
    aapxsClientInstances.add(uri, std::make_unique<AAPXSClientInstance>(AAPXSClientInstance{owner, uri, owner->instance_id, nullptr, dataCapacity}));
    auto ret = aapxsClientInstances.get(uri);
    ret->extension_message = staticSendExtensionMessage;
    ret->handle_extension_reply = staticProcessExtensionReply;
    return ret;
}

void aap::RemoteAAPXSManager::staticSendExtensionMessage(AAPXSClientInstance* clientInstance, int32_t messageSize, int32_t opcode) {
    auto thisObj = (RemotePluginInstance*) clientInstance->host_context;
    thisObj->sendExtensionMessage(clientInstance->uri, messageSize, opcode);
}

void
aap::RemoteAAPXSManager::staticProcessExtensionReply(AAPXSClientInstance *clientInstance, int32_t messageSize,
                                            int32_t opcode, int32_t requestId) {
    auto thisObj = (RemotePluginInstance*) clientInstance->host_context;
    thisObj->processExtensionReply(clientInstance->uri, messageSize, opcode, requestId);
}

void aap::RemotePluginInstance::sendExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize) {
    int32_t requestId = aapxsSysEx8RequestSerial();
    auto aapxs = getPluginAAPXSInstance(uri);
    assert(aapxs != nullptr);
    AAPXSRequestContext context{nullptr, nullptr, aapxs->serialization, requestId, opcode};
    assert(aapxs->serialization->data_capacity >= dataSize);
    memcpy(aapxs->serialization->data, data, dataSize);
    aapxs->serialization->data_size = dataSize;
    aapxs->send_aapxs_request(aapxs, &context);
}

AAPXSInitiatorInstance *aap::RemotePluginInstance::getPluginAAPXSInstance(const char *uri) {

    // FIXME: implement
    return nullptr;
}

AAPXSRecipientInstance *aap::RemotePluginInstance::getHostAAPXSInstance(const char *uri) {
    // FIXME: implement
    return nullptr;
}
