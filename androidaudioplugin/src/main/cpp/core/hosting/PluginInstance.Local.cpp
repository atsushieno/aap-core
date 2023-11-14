#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"

#define LOG_TAG "AAP.Local.Instance"


void aapxsProcessorAddEventUmpOutput(aap::AAPXSMidi2RecipientSession* processor, void* context, int32_t messageSize) {
    auto instance = (aap::LocalPluginInstance *) context;
    instance->addEventUmpOutput(processor->midi2_aapxs_data_buffer, messageSize);
}

aap::LocalPluginInstance::LocalPluginInstance(
        PluginHost *host,
        xs::AAPXSDefinitionRegistry *aapxsRegistry,
        int32_t instanceId,
        const PluginInformation* pluginInformation,
        AndroidAudioPluginFactory* loadedPluginFactory,
        int32_t sampleRate,
        int32_t eventMidi2InputBufferSize)
        : PluginInstance(pluginInformation, loadedPluginFactory, sampleRate, eventMidi2InputBufferSize),
          host(host),
          aapxs_host_session(eventMidi2InputBufferSize),
          feature_registry(new xs::AAPXSDefinitionServiceRegistry(aapxsRegistry)),
          aapxs_dispatcher(aapxsRegistry)
          {
    shared_memory_store = new aap::ServicePluginSharedMemoryStore();
    instance_id = instanceId;
    aapxs_out_midi2_buffer = calloc(1, event_midi2_buffer_size);
    aapxs_out_merge_buffer = calloc(1, event_midi2_buffer_size);

    aapxs_midi2_in_session.setExtensionCallback([&](aap_midi2_aapxs_parse_context* context) {
        handleAAPXSInput(context);
    });
}

aap::LocalPluginInstance::~LocalPluginInstance() {
    if (aapxs_out_midi2_buffer)
        free(aapxs_out_midi2_buffer);
    if (aapxs_out_merge_buffer)
        free(aapxs_out_merge_buffer);
}

AndroidAudioPluginHost* aap::LocalPluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = internalGetHostExtension;
    plugin_host_facade.request_process = internalRequestProcess;
    return &plugin_host_facade;
}

void *
aap::LocalPluginInstance::internalGetHostExtension(AndroidAudioPluginHost *host, const char *uri) {
    auto instance = (LocalPluginInstance *) host->context;
    // FIXME: in the future maybe we want to eliminate this kind of special cases.
    if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
        instance->host_plugin_info.get = get_plugin_info;
        return &instance->host_plugin_info;
    }
    // Look up host extension and get proxy via AAPXSDefinition.
    auto definition = instance->getAAPXSRegistry()->items()->getByUri(uri);
    if (definition && definition->get_host_extension_proxy) {
        auto aapxsInstance = instance->getAAPXSDispatcher().getHostAAPXSByUri(uri);
        auto proxy = definition->get_host_extension_proxy(definition, aapxsInstance, aapxsInstance->serialization);
        return proxy.as_host_extension(&proxy);
    }
    return nullptr;
}

void aap::LocalPluginInstance::internalRequestProcess(AndroidAudioPluginHost *host) {
    auto instance = (LocalPluginInstance *) host->context;
    instance->requestProcessToHost();
}

void aap::LocalPluginInstance::confirmPorts() {
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

void aap::LocalPluginInstance::requestProcessToHost() {
    if (process_requested_to_host)
        return;
    process_requested_to_host = true;
    ((PluginService*) host)->requestProcessToHost(instance_id);
}

void aap::LocalPluginInstance::addEventUmpOutput(void *input, int32_t size) {
    // unlike client side, we are not multithreaded during the audio processing,
    // but multiple async extension calls may race, so lock here too.
    const std::lock_guard<NanoSleepLock> lock{aapxs_out_merger_mutex_out};
    if (aapxs_out_midi2_buffer_offset + size > event_midi2_buffer_size)
        return;
    memcpy((uint8_t *) aapxs_out_midi2_buffer + aapxs_out_midi2_buffer_offset,
           input, size);
    aapxs_out_midi2_buffer_offset += size;
}

const char* local_trace_name = "AAP::LocalPluginInstance_process";
void aap::LocalPluginInstance::process(int32_t frameCount, int32_t timeoutInNanoseconds) {
    process_requested_to_host = false;

    struct timespec timeSpecBegin{}, timeSpecEnd{};
#if ANDROID
    if (ATrace_isEnabled()) {
        ATrace_beginSection(local_trace_name);
        clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
    }
#endif

    if (std::unique_lock<NanoSleepLock> tryLock(ump_sequence_merger_mutex, std::try_to_lock); tryLock.owns_lock()) {
        // merge input from native UI into the host's MIDI inputs
        merge_ump_sequences(AAP_PORT_DIRECTION_INPUT, event_midi2_merge_buffer, event_midi2_buffer_size,
                            event_midi2_buffer, event_midi2_buffer_offset,
                            getAudioPluginBuffer(), this);
        event_midi2_buffer_offset = 0;
    }

    // retrieve AAPXS SysEx8 requests and start extension calls, if any.
    // (might be synchronously done)
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
            continue;
        auto aapBuffer = getAudioPluginBuffer();
        void *data = aapBuffer->get_buffer(*aapBuffer, i);
        aapxs_midi2_in_session.process(data);
    }

    plugin->process(plugin, getAudioPluginBuffer(), frameCount, timeoutInNanoseconds);

    // before sending back to host, merge AAPXS SysEx8 UMPs from async extension calls
    // into the plugin's MIDI output buffer.
    if (std::unique_lock<NanoSleepLock> tryLock(aapxs_out_merger_mutex_out, std::try_to_lock); tryLock.owns_lock()) {
        merge_ump_sequences(AAP_PORT_DIRECTION_OUTPUT, aapxs_out_merge_buffer, event_midi2_buffer_size,
                            aapxs_out_midi2_buffer, aapxs_out_midi2_buffer_offset,
                            getAudioPluginBuffer(), this);
        aapxs_out_midi2_buffer_offset = 0;
    }

#if ANDROID
    if (ATrace_isEnabled()) {
        clock_gettime(CLOCK_REALTIME, &timeSpecEnd);
        ATrace_setCounter(local_trace_name,
                          (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 + timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec);
        ATrace_endSection();
    }
#endif
}

// ---- AAPXS v2

void aap::LocalPluginInstance::setupAAPXS() {
    standards = std::make_unique<xs::ServiceStandardExtensions>(plugin);
}

static inline void staticSendAAPXSReply(AAPXSRecipientInstance* instance, AAPXSRequestContext* context) {
    ((aap::LocalPluginInstance *) instance->host_context)->sendPluginAAPXSReply(context);
}
static inline bool staticSendAAPXSRequest(AAPXSInitiatorInstance* instance, AAPXSRequestContext* context) {
    return ((aap::LocalPluginInstance*) instance->host_context)->sendHostAAPXSRequest(context);
}

void aap::LocalPluginInstance::setupAAPXSInstances() {
    auto store = getSharedMemoryStore();
    auto func = [&](const char* uri, AAPXSSerializationContext* serialization) {
        if (feature_registry->items()->getByUri(uri)->data_capacity == 0)
            return; // no need to allocate serialization data
        auto index = store->getExtensionUriToIndexMap()[uri];
        serialization->data = store->getExtensionBuffer(index);
        serialization->data_capacity = store->getExtensionBufferCapacity(index);
    };
    aapxs_dispatcher.setupInstances(this,
                                    func,
                                    staticSendAAPXSReply,
                                    staticSendAAPXSRequest,
                                    staticGetNewRequestId);
}

void
aap::LocalPluginInstance::sendPluginAAPXSReply(AAPXSRequestContext* request) {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        aapxs_midi2_in_session.addReply(aapxsProcessorAddEventUmpOutput,
                                        this,
                                        request->uri,
                                       // should we support MIDI 2.0 group?
                                       0,
                                        request->request_id,
                                        request->serialization->data,
                                        request->serialization->data_size,
                                        request->opcode);
    } else {
        // it is synchronously handled at Binder IPC, nothing to process here.
    }
}

bool
aap::LocalPluginInstance::sendHostAAPXSRequest(AAPXSRequestContext* request) {
    // If it is at ACTIVE state it has to switch to AAPXS SysEx8 MIDI messaging mode,
    // otherwise it goes to the Binder route.
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // aapxsInstance already contains binary data here, so we retrieve data from there.
        // This is an asynchronous function, so we do not wait for the result.
        aapxs_host_session.addSession(aapxsSessionAddEventUmpInput, this, request);
        return true;
    } else {
        // the actual implementation is in AudioPluginInterfaceImpl, kicks `hostExtension()` on the callback proxy object.
        ipc_send_extension_message_func(ipc_send_extension_message_context, request->uri, getInstanceId(), request->opcode);
        return false;
    }
}

void aap::LocalPluginInstance::controlExtension(uint8_t urid, const std::string &uri, int32_t opcode, uint32_t requestId)  {
    auto registry = feature_registry.get()->items();
    auto def = urid != 0 ? registry->getByUrid(urid) : registry->getByUri(uri.c_str());

    if (def) { // ignore undefined extensions here
        auto& dispatcher = getAAPXSDispatcher();
        auto instance = urid != 0 ? dispatcher.getPluginAAPXSByUrid(urid) : dispatcher.getPluginAAPXSByUri(uri.c_str());
        AAPXSRequestContext context{nullptr, nullptr, instance->serialization, 0, uri.c_str(), requestId, opcode};
        def->process_incoming_plugin_aapxs_request(def, instance, plugin, &context);
    }
}

void aap::LocalPluginInstance::handleAAPXSInput(aap_midi2_aapxs_parse_context *context) {
    if (context->opcode >= 0) {
        // plugin request
        auto& dispatcher = getAAPXSDispatcher();
        auto aapxsInstance = context->urid != 0 ? dispatcher.getPluginAAPXSByUrid(context->urid) : dispatcher.getPluginAAPXSByUri(context->uri);
        // We need to copy extension data buffer before calling it.
        memcpy(aapxsInstance->serialization->data, (int32_t*) context->data, context->dataSize);
        controlExtension(context->urid, context->uri, context->opcode, context->request_id);
/*
        // FIXME: this should be called only at the *end* of controlExtension()
        //  which should be asynchronously handled.
        aapxs_midi2_in_session.addReply(aapxsProcessorAddEventUmpOutput,
                                        this,
                                        context->uri,
                                        context->group,
                                        context->request_id,
                                        aapxsInstance->serialization->data,
                                        context->dataSize,
                                        context->opcode
        );
*/
    } else {
        // host reply
        auto& dispatcher = getAAPXSDispatcher();
        auto aapxsInstance = context->urid != 0 ? dispatcher.getHostAAPXSByUrid(context->urid) : dispatcher.getHostAAPXSByUri(context->uri);
        // We need to copy extension data buffer before calling it.
        memcpy(aapxsInstance->serialization->data, (int32_t*) context->data, context->dataSize);

        auto registry = feature_registry.get()->items();
        auto def = context->urid != 0 ? registry->getByUrid(context->urid) : registry->getByUri(context->uri);
        if (def) { // ignore undefined extensions here
            AAPXSRequestContext request{nullptr, nullptr, aapxsInstance->serialization,
                                        context->urid, context->uri, context->request_id, context->opcode};
            def->process_incoming_host_aapxs_reply(def, aapxsInstance, &plugin_host_facade, &request);
        }
    }
}
