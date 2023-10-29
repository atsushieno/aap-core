#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"

#define LOG_TAG "AAP.Local.Instance"


void aapxsProcessorAddEventUmpOutput(aap::AAPXSMidi2Processor* processor, void* context, int32_t messageSize) {
    auto instance = (aap::LocalPluginInstance *) context;
    instance->addEventUmpOutput(processor->midi2_aapxs_data_buffer, messageSize);
}

aap::LocalPluginInstance::LocalPluginInstance(PluginHost *host,
                                         AAPXSRegistry *aapxsRegistry,
                                         int32_t instanceId,
                                         const PluginInformation* pluginInformation,
                                         AndroidAudioPluginFactory* loadedPluginFactory,
                                         int32_t sampleRate,
                                         int32_t eventMidi2InputBufferSize)
        : PluginInstance(pluginInformation, loadedPluginFactory, sampleRate, eventMidi2InputBufferSize),
          host(host),
          aapxs_registry(aapxsRegistry),
          aapxsServiceInstances([&]() { return getPlugin(); }),
          standards(this),
          feature_registry(new AAPXSFeatureRegistryServiceImpl(this)),
          instance_manager(AAPXSServiceInstanceManagerVNext(feature_registry.get())) {
    shared_memory_store = new aap::ServicePluginSharedMemoryStore();
    instance_id = instanceId;
    aapxs_out_midi2_buffer = calloc(1, event_midi2_buffer_size);
    aapxs_out_merge_buffer = calloc(1, event_midi2_buffer_size);

    aapxs_midi2_processor.setExtensionCallback([&](aap_midi2_aapxs_parse_context* context) {
        auto aapxsInstance = aapxsServiceInstances.get(context->uri, false);
        // We need to copy extension data buffer before calling it.
        memcpy(aapxsInstance->data, (int32_t*) context->data, context->dataSize);
        controlExtension(context->uri, context->opcode);

        // FIXME: this should be called only at the *end* of controlExtension()
        //  which should be asynchronously handled.
        aapxs_midi2_processor.addReply(aapxsProcessorAddEventUmpOutput,
                                       this,
                                       context->group,
                                       context->request_id,
                                       aapxsInstance,
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
    if (strcmp(uri, AAP_PARAMETERS_EXTENSION_URI) == 0) {
        auto instance = (LocalPluginInstance *) host->context;
        instance->host_parameters_extension.notify_parameters_changed = notify_parameters_changed;
        return &instance->host_parameters_extension;
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

AAPXSServiceInstance* aap::LocalPluginInstance::setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) {
    const char *uri = aapxsServiceInstances.addOrGetUri(feature->uri);
    assert(aapxsServiceInstances.get(uri) == nullptr);
    if (dataSize < 0)
        dataSize = feature->shared_memory_size;
    aapxsServiceInstances.add(uri, std::make_unique<AAPXSServiceInstance>(
            AAPXSServiceInstance{this, uri, getInstanceId(), nullptr, dataSize}));
    return aapxsServiceInstances.get(uri);
}

void aap::LocalPluginInstance::notify_parameters_changed(aap_host_parameters_extension_t* ext, AndroidAudioPluginHost *host,
                                                    AndroidAudioPlugin *plugin) {
    assert(false); // FIXME: implement
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

void aap::LocalPluginInstance::setupAAPXSServiceInstance(aap::AAPXSServiceFeatureRegistry *registry,
                                                         aap::AAPXSServiceInstanceManagerVNext *serviceInstances,
                                                         AAPXSSerializationContext *serialization) {
    std::for_each(registry->begin(), registry->end(), [&](AAPXSFeatureVNext* f) {
        serviceInstances->addInitiator(populateAAPXSInitiatorInstance(serialization), f->uri);
        serviceInstances->addRecipient(populateAAPXSRecipientInstance(serialization), f->uri);
    });
}

AAPXSRecipientInstance
aap::LocalPluginInstance::populateAAPXSRecipientInstance(AAPXSSerializationContext *serialization) {
    AAPXSRecipientInstance instance{this,
                                    serialization,
                                    staticGetNewRequestId,
                                    staticProcessIncomingAAPXSRequest,
                                    staticSendAAPXSReply};
    return instance;
}

AAPXSInitiatorInstance
aap::LocalPluginInstance::populateAAPXSInitiatorInstance(AAPXSSerializationContext *serialization) {
    AAPXSInitiatorInstance instance{this,
                                    serialization,
                                    staticGetNewRequestId,
                                    staticSendHostAAPXSRequest,
                                    staticProcessIncomingHostAAPXSReply};
    return instance;
}

void aap::LocalPluginInstance::processExtensionRequest(const char *uri, int32_t messageSize,
                                                       int32_t opcode, int32_t requestId) {
    auto aapxs = getPluginAAPXSInstance(uri);
    assert(aapxs != nullptr);
    AAPXSRequestContext context{nullptr, nullptr, aapxs->serialization, uri, requestId, opcode};
    aapxs->process_incoming_aapxs_request(aapxs, &context);
}

void aap::LocalPluginInstance::sendHostExtensionRequest(const char* uri, int32_t opcode, void *data, int32_t dataSize) {
    int32_t requestId = aapxsRequestIdSerial();
    auto aapxs = getHostAAPXSInstance(uri);
    assert(aapxs != nullptr);
    AAPXSRequestContext context{nullptr, nullptr, aapxs->serialization, uri, requestId, opcode};
    assert(aapxs->serialization->data_capacity >= dataSize);
    memcpy(aapxs->serialization->data, data, dataSize);
    aapxs->serialization->data_size = dataSize;
    aapxs->send_aapxs_request(aapxs, &context);
}

void aap::LocalPluginInstance::processHostExtensionReply(const char *uri, int32_t messageSize,
                                                         int32_t opcode, int32_t requestId) {
    // FIXME: implement
    //  should be almost the same as client plugin extension reply handler (that means, we need aapxs_session here too)
    throw std::runtime_error("FIXME: implement");
}

void aap::LocalPluginInstance::sendExtensionReply(const char *uri, int32_t opcode, void *data,
                                                       int32_t dataSize, int32_t requestId) {
    auto aapxs = getPluginAAPXSInstance(uri);
    assert(aapxs != nullptr);
    AAPXSRequestContext context{nullptr, nullptr, aapxs->serialization, uri, requestId, opcode};
    assert(aapxs->serialization->data_capacity >= dataSize);
    memcpy(aapxs->serialization->data, data, dataSize);
    aapxs->serialization->data_size = dataSize;
    aapxs->send_aapxs_reply(aapxs, &context);
}

AAPXSRecipientInstance* aap::LocalPluginInstance::getPluginAAPXSInstance(const char *uri) {
    return &instance_manager.getPluginAAPXSByUri(uri);
}

AAPXSRecipientInstance* aap::LocalPluginInstance::getPluginAAPXSInstance(int32_t urid) {
    return &instance_manager.getPluginAAPXSByUrid(urid);
}

AAPXSInitiatorInstance *aap::LocalPluginInstance::getHostAAPXSInstance(const char *uri) {
    return &instance_manager.getHostAAPXSByUri(uri);
}

AAPXSInitiatorInstance *aap::LocalPluginInstance::getHostAAPXSInstance(int32_t urid) {
    return &instance_manager.getHostAAPXSByUrid(urid);
}
