#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"

#define LOG_TAG "AAP.Local.Instance"


void aapxsProcessorAddEventUmpOutput(aap::AAPXSMidi2ReceiverSession* processor, void* context, int32_t messageSize) {
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

    aapxs_midi2_processor.setExtensionCallback([&](aap_midi2_aapxs_parse_context* context) {
        auto aapxsInstance = getAAPXSDispatcher().getPluginAAPXSByUri(context->uri);
        // We need to copy extension data buffer before calling it.
        memcpy(aapxsInstance->serialization->data, (int32_t*) context->data, context->dataSize);
        controlExtension(context->uri, context->opcode, context->request_id);

        // FIXME: this should be called only at the *end* of controlExtension()
        //  which should be asynchronously handled.
        aapxs_midi2_processor.addReply(aapxsProcessorAddEventUmpOutput,
                                       this,
                                       context->uri,
                                       context->group,
                                       context->request_id,
                                       aapxsInstance->serialization->data,
                                       context->dataSize,
                                       context->opcode
        );
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
    if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
        auto instance = (LocalPluginInstance *) host->context;
        instance->host_plugin_info.get = get_plugin_info;
        return &instance->host_plugin_info;
    }
    // FIXME: implement more extensions
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
        aapxs_midi2_processor.process(data);
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
    ((aap::LocalPluginInstance *) instance->host_context)->sendPluginAAPXSReply(context->uri,
                                                                                context->opcode,
                                                                                context->serialization->data,
                                                                                context->serialization->data_size,
                                                                                context->request_id);
}
static inline bool staticSendAAPXSRequest(AAPXSInitiatorInstance* instance, AAPXSRequestContext* context) {
    return ((aap::LocalPluginInstance*) instance->host_context)->sendHostAAPXSRequest(context->uri, context->opcode, context->serialization->data, context->serialization->data_size, context->request_id);
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
aap::LocalPluginInstance::sendPluginAAPXSReply(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t requestId) {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        auto aapxsInstance = getAAPXSDispatcher().getPluginAAPXSByUri(uri);
        aapxs_midi2_processor.addReply(aapxsProcessorAddEventUmpOutput,
                                       this,
                                       uri,
                                       // should we support MIDI 2.0 group?
                                       0,
                                       requestId,
                                       aapxsInstance->serialization->data,
                                       dataSize,
                                       opcode);
    } else {
        // it is synchronously handled at Binder IPC, nothing to process here.
    }
}

bool
aap::LocalPluginInstance::sendHostAAPXSRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId) {
    auto aapxsInstance = aapxs_dispatcher.getHostAAPXSByUri(uri);

    // If it is at ACTIVE state it has to switch to AAPXS SysEx8 MIDI messaging mode,
    // otherwise it goes to the Binder route.
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // aapxsInstance already contains binary data here, so we retrieve data from there.
        int32_t group = 0; // will we have to give special semantics on it?
        // This is an asynchronous function, so we do not wait for the result.
        aapxs_host_session.addSession(aapxsSessionAddEventUmpInput, this, group, newRequestId, uri, aapxsInstance->serialization->data, dataSize, opcode);
        return true;
    } else {
        // the actual implementation is in AudioPluginInterfaceImpl, kicks `hostExtension()` on the callback proxy object.
        ipc_send_extension_message_func(ipc_send_extension_message_context, uri, getInstanceId(), opcode);
        return false;
    }
}

void aap::LocalPluginInstance::controlExtension(const std::string &uri, int32_t opcode, uint32_t requestId)  {
    auto def = feature_registry.get()->items()->getByUri(uri.c_str());
    assert(def != nullptr);
    auto instance = getAAPXSDispatcher().getPluginAAPXSByUri(uri.c_str());
    AAPXSRequestContext context{nullptr, nullptr, instance->serialization, 0, uri.c_str(), requestId, opcode};
    def->process_incoming_plugin_aapxs_request(def, instance, plugin, &context);
}
