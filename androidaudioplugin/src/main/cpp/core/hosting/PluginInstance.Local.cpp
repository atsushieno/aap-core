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
          standards(this) {
    shared_memory_store = new aap::ServicePluginSharedMemoryStore();
    instance_id = instanceId;

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

AndroidAudioPluginHost* aap::LocalPluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = internalGetExtension;
    plugin_host_facade.request_process = internalRequestProcess;
    return &plugin_host_facade;
}

void *
aap::LocalPluginInstance::internalGetExtension(AndroidAudioPluginHost *host, const char *uri) {
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

// plugin-info host extension implementation.
static uint32_t plugin_info_port_get_index(aap_plugin_info_port_t* port) { return ((aap::PortInformation*) port->context)->getIndex(); }
static const char* plugin_info_port_get_name(aap_plugin_info_port_t* port) { return ((aap::PortInformation*) port->context)->getName(); }
static aap_content_type plugin_info_port_get_content_type(aap_plugin_info_port_t* port) { return (aap_content_type) ((aap::PortInformation*) port->context)->getContentType(); }
static aap_port_direction plugin_info_port_get_direction(aap_plugin_info_port_t* port) { return (aap_port_direction) ((aap::PortInformation*) port->context)->getPortDirection(); }

static aap_plugin_info_port_t plugin_info_get_port(aap_plugin_info_t* plugin, uint32_t index) {
    auto port = ((aap::LocalPluginInstance*) plugin->context)->getPort(index);
    return aap_plugin_info_port_t{(void *) port,
                                  plugin_info_port_get_index,
                                  plugin_info_port_get_name,
                                  plugin_info_port_get_content_type,
                                  plugin_info_port_get_direction};
}

static const char* plugin_info_get_plugin_package_name(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getPluginPackageName().c_str(); }
static const char* plugin_info_get_plugin_local_name(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getPluginLocalName().c_str(); }
static const char* plugin_info_get_display_name(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getDisplayName().c_str(); }
static const char* plugin_info_get_developer_name(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getDeveloperName().c_str(); }
static const char* plugin_info_get_version(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getVersion().c_str(); }
static const char* plugin_info_get_primary_category(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getPrimaryCategory().c_str(); }
static const char* plugin_info_get_identifier_string(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getStrictIdentifier().c_str(); }
static const char* plugin_info_get_plugin_id(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getPluginInformation()->getPluginID().c_str(); }
static uint32_t plugin_info_get_port_count(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getNumPorts(); }
static uint32_t plugin_info_get_parameter_count(aap_plugin_info_t* plugin) { return ((aap::LocalPluginInstance*) plugin->context)->getNumParameters(); }

static uint32_t plugin_info_parameter_get_id(aap_plugin_info_parameter_t* parameter) { return ((aap::ParameterInformation*) parameter->context)->getId(); }
static const char* plugin_info_parameter_get_name(aap_plugin_info_parameter_t* parameter) { return ((aap::ParameterInformation*) parameter->context)->getName(); }
static float plugin_info_parameter_get_min_value(aap_plugin_info_parameter_t* parameter) { return (aap_content_type) ((aap::ParameterInformation*) parameter->context)->getMinimumValue(); }
static float plugin_info_parameter_get_max_value(aap_plugin_info_parameter_t* parameter) { return (aap_content_type) ((aap::ParameterInformation*) parameter->context)->getMaximumValue(); }
static float plugin_info_parameter_get_default_value(aap_plugin_info_parameter_t* parameter) { return (aap_content_type) ((aap::ParameterInformation*) parameter->context)->getDefaultValue(); }

static aap_plugin_info_parameter_t plugin_info_get_parameter(aap_plugin_info_t* plugin, uint32_t index) {
    auto para = ((aap::LocalPluginInstance*) plugin->context)->getParameter(index);
    return aap_plugin_info_parameter_t{(void *) para,
                                       plugin_info_parameter_get_id,
                                       plugin_info_parameter_get_name,
                                       plugin_info_parameter_get_min_value,
                                       plugin_info_parameter_get_max_value,
                                       plugin_info_parameter_get_default_value};
}

aap_plugin_info_t aap::LocalPluginInstance::get_plugin_info(aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char* pluginId) {
    auto* instance = (LocalPluginInstance*) host->context;
    aap_plugin_info_t ret{(void*) instance,
                          plugin_info_get_plugin_package_name,
                          plugin_info_get_plugin_local_name,
                          plugin_info_get_display_name,
                          plugin_info_get_developer_name,
                          plugin_info_get_version,
                          plugin_info_get_primary_category,
                          plugin_info_get_identifier_string,
                          plugin_info_get_plugin_id,
                          plugin_info_get_port_count,
                          plugin_info_get_port,
                          plugin_info_get_parameter_count,
                          plugin_info_get_parameter};
    return ret;
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

    // retrieve AAPXS SysEx8 requests and start extension calls, if any.
    // (might be synchronously done)
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
            continue;
        auto aapBuffer = getAudioPluginBuffer();
        void* data = aapBuffer->get_buffer(*aapBuffer, i);
        aapxs_midi2_processor.process(data);
    }

    plugin->process(plugin, getAudioPluginBuffer(), frameCount, timeoutInNanoseconds);

    // before sending back to host, merge AAPXS SysEx8 UMPs from async extension calls
    // into the plugin's MIDI output buffer.
    if (std::unique_lock<NanoSleepLock> tryLock(ump_sequence_merger_mutex, std::try_to_lock); tryLock.owns_lock()) {
        merge_ump_sequences(AAP_PORT_DIRECTION_OUTPUT, event_midi2_merge_buffer, event_midi2_buffer_size,
                            event_midi2_buffer, event_midi2_buffer_offset,
                            getAudioPluginBuffer(), this);
        event_midi2_buffer_offset = 0;
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
