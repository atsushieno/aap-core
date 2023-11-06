
#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"

#define LOG_TAG "AAP.Instance"

aap::PluginInstance::PluginInstance(const PluginInformation* pluginInformation,
                               AndroidAudioPluginFactory* loadedPluginFactory,
                               int32_t sampleRate,
                               int32_t eventMidi2InputBufferSize)
        : sample_rate(sampleRate),
          plugin_factory(loadedPluginFactory),
          instantiation_state(PLUGIN_INSTANTIATION_STATE_INITIAL),
          plugin(nullptr),
          pluginInfo(pluginInformation),
          event_midi2_buffer_size(eventMidi2InputBufferSize) {
    assert(pluginInformation);
    assert(loadedPluginFactory);
    assert(event_midi2_buffer_size > 0);
    event_midi2_buffer = calloc(1, event_midi2_buffer_size);
    event_midi2_merge_buffer = calloc(1, event_midi2_buffer_size);
}

aap::PluginInstance::~PluginInstance() {
    instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
    if (plugin != nullptr)
        plugin_factory->release(plugin_factory, plugin);
    plugin = nullptr;
    delete shared_memory_store;
    if (event_midi2_buffer)
        free(event_midi2_buffer);
    if (event_midi2_merge_buffer)
        free(event_midi2_merge_buffer);
}

aap_buffer_t* aap::PluginInstance::getAudioPluginBuffer() {
    return shared_memory_store->getAudioPluginBuffer();
}

void aap::PluginInstance::completeInstantiation()
{
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_INITIAL) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to completeInstantiation() at state: %d (instanceId: %d)",
                     instantiation_state, instance_id);
        return;
    }

    AndroidAudioPluginHost* asPluginAPI = getHostFacadeForCompleteInstantiation();
    plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), sample_rate, asPluginAPI);
    assert(plugin);

    instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;

    setupAAPXS();
}

void aap::PluginInstance::setupPortConfigDefaults() {
    // If there is no declared ports, apply default ports configuration.
    uint32_t nPort = 0;

    // MIDI2 in/out ports are always populated
    PortInformation midi_in{nPort++, "MIDI In", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
    PortInformation midi_out{nPort++, "MIDI Out", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_OUTPUT};
    configured_ports->emplace_back(midi_in);
    configured_ports->emplace_back(midi_out);

    // Populate audio in ports only if it is not an instrument.
    // FIXME: there may be better ways to guess whether audio in ports are required or not.
    if (!pluginInfo->isInstrument()) {
        PortInformation audio_in_l{nPort++, "Audio In L", AAP_CONTENT_TYPE_AUDIO, AAP_PORT_DIRECTION_INPUT};
        configured_ports->emplace_back(audio_in_l);
        PortInformation audio_in_r{nPort++, "Audio In R", AAP_CONTENT_TYPE_AUDIO, AAP_PORT_DIRECTION_INPUT};
        configured_ports->emplace_back(audio_in_r);
    }
    PortInformation audio_out_l{nPort++, "Audio Out L", AAP_CONTENT_TYPE_AUDIO, AAP_PORT_DIRECTION_OUTPUT};
    configured_ports->emplace_back(audio_out_l);
    PortInformation audio_out_r{nPort++, "Audio Out R", AAP_CONTENT_TYPE_AUDIO, AAP_PORT_DIRECTION_OUTPUT};
    configured_ports->emplace_back(audio_out_r);
}


// This means that there was no configured ports by extensions.
void aap::PluginInstance::setupPortsViaMetadata() {
    if (are_ports_configured)
        return;
    are_ports_configured = true;

    for (int i = 0, n = pluginInfo->getNumDeclaredPorts(); i < n; i++)
        configured_ports->emplace_back(PortInformation{*pluginInfo->getDeclaredPort(i)});
}

void aap::PluginInstance::startPortConfiguration() {
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

void aap::PluginInstance::scanParametersAndBuildList() {
    assert(!cached_parameters);

    auto ext = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
    if (!ext || !ext->get_parameter_count || !ext->get_parameter)
        return;

    auto parameterCount = ext->get_parameter_count(ext, plugin);
    if (parameterCount == -1) // explicitly indicates that the code is not going to return the parameter list.
        return;

    // if parameters extension does not exist, do not populate cached parameters list.
    // (The empty list means no parameters in metadata either.)
    cached_parameters = std::make_unique<std::vector<ParameterInformation>>();

    for (auto i = 0; i < parameterCount; i++) {
        auto para = ext->get_parameter(ext, plugin, i);
        ParameterInformation p{para.stable_id, para.display_name, para.min_value, para.max_value, para.default_value};
        if (ext->get_enumeration_count && ext->get_enumeration) {
            for (auto e = 0, en = ext->get_enumeration_count(ext, plugin, i); e < en; e++) {
                auto pe = ext->get_enumeration(ext, plugin, i, e);
                ParameterInformation::Enumeration eDef{e, pe.value, pe.name};
                p.addEnumeration(eDef);
            }
        }
        cached_parameters->emplace_back(p);
    }
}

void aap::PluginInstance::activate() {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
        return;
    assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);

    plugin->activate(plugin);
    instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
}

void aap::PluginInstance::deactivate() {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE ||
        instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED)
        return;
    assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);

    plugin->deactivate(plugin);
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
}


void aap::PluginInstance::addEventUmpInput(void *input, int32_t size) {
    const std::lock_guard<NanoSleepLock> lock{ump_sequence_merger_mutex};
    if (event_midi2_buffer_offset + size > event_midi2_buffer_size)
        return;
    memcpy((uint8_t *) event_midi2_buffer + event_midi2_buffer_offset,
           input, size);
    event_midi2_buffer_offset += size;
}

void aap::PluginInstance::merge_ump_sequences(aap_port_direction portDirection, void *mergeTmp, int32_t mergeBufSize, void* sequence, int32_t sequenceSize, aap_buffer_t *buffer, PluginInstance* instance) {
    if (sequenceSize == 0)
        return;
    for (int i = 0; i < instance->getNumPorts(); i++) {
        auto port = instance->getPort(i);
        if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2 && port->getPortDirection() == portDirection) {
            auto mbh = (AAPMidiBufferHeader*) buffer->get_buffer(*buffer, i);
            size_t newSize = cmidi2_ump_merge_sequences((cmidi2_ump*) mergeTmp, mergeBufSize,
                                                        (cmidi2_ump*) sequence, (size_t) sequenceSize,
                                                        (cmidi2_ump*) (mbh + 1), (size_t) mbh->length);
            mbh->length = newSize;
            if (newSize > 0)
                memcpy(mbh + 1, mergeTmp, newSize);
            return;
        }
    }
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

static const char* plugin_info_get_plugin_package_name(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getPluginPackageName().c_str(); }
static const char* plugin_info_get_plugin_local_name(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getPluginLocalName().c_str(); }
static const char* plugin_info_get_display_name(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getDisplayName().c_str(); }
static const char* plugin_info_get_developer_name(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getDeveloperName().c_str(); }
static const char* plugin_info_get_version(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getVersion().c_str(); }
static const char* plugin_info_get_primary_category(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getPrimaryCategory().c_str(); }
static const char* plugin_info_get_identifier_string(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getStrictIdentifier().c_str(); }
static const char* plugin_info_get_plugin_id(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getPluginInformation()->getPluginID().c_str(); }
static uint32_t plugin_info_get_port_count(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getNumPorts(); }
static uint32_t plugin_info_get_parameter_count(aap_plugin_info_t* plugin) { return ((aap::PluginInstance*) plugin->context)->getNumParameters(); }

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

aap_plugin_info_t aap::PluginInstance::get_plugin_info(aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char* pluginId) {
    auto* instance = (PluginInstance*) host->context;
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

uint32_t aapxs_request_id_serial{0};
uint32_t aap::PluginInstance::aapxsRequestIdSerial() {
    return aapxs_request_id_serial++;
}

// AAPXS (v2 too)

void aap::PluginInstance::aapxsSessionAddEventUmpInput(aap::AAPXSMidi2ClientSession* client, void* context, int32_t messageSize) {
    auto instance = (aap::RemotePluginInstance *) context;
    instance->addEventUmpInput(client->aapxs_rt_midi_buffer, messageSize);
}
