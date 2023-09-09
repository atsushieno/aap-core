#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"

#define LOG_TAG "AAP.Local.Instance"

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
        auto aapxsInstance = aapxsServiceInstances.get(context->uri, true);
        // We need to copy extension data buffer before calling it.
        memcpy(aapxsInstance->data, context->data, context->dataSize);
        controlExtension(context->uri, *(int32_t*) (context->data));
    });
}

AndroidAudioPluginHost* aap::LocalPluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = internalGetExtension;
    plugin_host_facade.request_process = internalRequestProcess;
    return &plugin_host_facade;
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
    auto instance = (LocalPluginInstance*) host->context;
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

void aap::LocalPluginInstance::processAAPXSSysEx8Input() {
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
            continue;
        auto aapBuffer = getAudioPluginBuffer();
        void* data = aapBuffer->get_buffer(*aapBuffer, i);
        aapxs_midi2_processor.process(data);
    }
}