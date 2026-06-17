
#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"
#include "aap/ext/midi.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "../include_cmidi2.h"

#define LOG_TAG "AAP.Instance"

namespace {
struct PluginParameterState {
    aap_parameters_host_extension_t host_parameters{};
    aap::NanoSleepLock mutex{};
    std::vector<double> values{};
    std::unordered_map<int32_t, int32_t> id_to_index{};
    std::atomic<uint32_t> revision{1};
};

std::mutex parameter_state_registry_mutex;
std::unordered_map<aap::PluginInstance*, std::unique_ptr<PluginParameterState>> parameter_state_registry;

PluginParameterState* get_parameter_state(aap::PluginInstance* instance, bool create = true) {
    const std::lock_guard<std::mutex> lock{parameter_state_registry_mutex};
    auto it = parameter_state_registry.find(instance);
    if (it != parameter_state_registry.end())
        return it->second.get();
    if (!create)
        return nullptr;
    auto state = std::make_unique<PluginParameterState>();
    auto* ret = state.get();
    parameter_state_registry[instance] = std::move(state);
    return ret;
}
}

aap::PluginInstance::PluginInstance(const PluginInformation* pluginInformation,
                               AndroidAudioPluginFactory* loadedPluginFactory,
                               int32_t eventMidi2InputBufferSize)
        : plugin_factory(loadedPluginFactory),
          instantiation_state(PLUGIN_INSTANTIATION_STATE_INITIAL),
          plugin(nullptr),
          pluginInfo(pluginInformation),
        event_midi2_buffer_size(eventMidi2InputBufferSize) {
    if (!pluginInformation)
        AAP_ASSERT_FALSE; // should not happen
    if (!loadedPluginFactory)
        AAP_ASSERT_FALSE; // should not happen
    if (event_midi2_buffer_size <= 0)
        AAP_ASSERT_FALSE; // should not happen
    else {
        event_midi2_buffer = calloc(1, event_midi2_buffer_size);
        event_midi2_merge_buffer = calloc(1, event_midi2_buffer_size);
    }
}

static void rebuild_parameter_id_index(aap::PluginInstance* instance,
                                       std::unordered_map<int32_t, int32_t>& idToIndex) {
    idToIndex.clear();
    for (int32_t i = 0, n = instance->getNumParameters(); i < n; ++i) {
        auto* parameter = instance->getParameter(i);
        if (parameter)
            idToIndex[parameter->getId()] = i;
    }
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
    const std::lock_guard<std::mutex> lock{parameter_state_registry_mutex};
    parameter_state_registry.erase(this);
}

aap_buffer_t* aap::PluginInstance::getAudioPluginBuffer() {
    return shared_memory_store ? shared_memory_store->getAudioPluginBuffer() : nullptr;
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
    plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), asPluginAPI);
    if (plugin) {
        instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
    } else {
        AAP_ASSERT_FALSE;
        instantiation_state = PLUGIN_INSTANTIATION_STATE_ERROR;
    }
}

void aap::PluginInstance::setupPortConfigDefaults() {
    // If there is no declared ports, apply default ports configuration.
    uint32_t nPort = 0;

    // Populate audio in ports only if it is not an instrument.
    // FIXME: there may be better ways to guess whether audio in ports are required or not.
    if (!pluginInfo->isInstrument()) {
        // create audio inputs for Effect plugins
        PortInformation audio_in_l{nPort++, "Audio In L", AAP_CONTENT_TYPE_AUDIO,
                                   AAP_PORT_DIRECTION_INPUT};
        configured_ports->emplace_back(audio_in_l);
        PortInformation audio_in_r{nPort++, "Audio In R", AAP_CONTENT_TYPE_AUDIO,
                                   AAP_PORT_DIRECTION_INPUT};
        configured_ports->emplace_back(audio_in_r);
    }
    PortInformation audio_out_l{nPort++, "Audio Out L", AAP_CONTENT_TYPE_AUDIO,
                                AAP_PORT_DIRECTION_OUTPUT};
    configured_ports->emplace_back(audio_out_l);
    PortInformation audio_out_r{nPort++, "Audio Out R", AAP_CONTENT_TYPE_AUDIO,
                                AAP_PORT_DIRECTION_OUTPUT};
    configured_ports->emplace_back(audio_out_r);

    // MIDI2 in/out ports are always populated
    // create System MIDI input for Instrument plugins. We always need one for AAPXS.
    const char* midiInName = pluginInfo->isInstrument() ? "MIDI In" : "System MIDI In";
    PortInformation midi_in{nPort++, midiInName, AAP_CONTENT_TYPE_MIDI2,
                            AAP_PORT_DIRECTION_INPUT};
    configured_ports->emplace_back(midi_in);
    PortInformation midi_out{nPort++, "System MIDI Out", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_OUTPUT};
    configured_ports->emplace_back(midi_out);

}


// This means that there was no configured ports by extensions.
void aap::PluginInstance::setupPortsViaMetadata() {
    if (are_ports_configured)
        return;
    are_ports_configured = true;

    bool hasMidiIn = false, hasMidiOut = false;
    for (int i = 0, n = pluginInfo->getNumDeclaredPorts(); i < n; i++) {
        auto port = *pluginInfo->getDeclaredPort(i);
        configured_ports->emplace_back(PortInformation{port});
        if (port.getContentType() == AAP_CONTENT_TYPE_MIDI2) {
            if (port.getPortDirection() == AAP_PORT_DIRECTION_INPUT)
                hasMidiIn = true;
            else
                hasMidiOut = true;
        }
    }

    // MIDI2 in/out ports are always populated for AAPXS SysEx8 messaging,
    //  and parameter changes (FIXME: which should be optional in theory?)
    if (!hasMidiIn) {
        PortInformation midi_in{(uint32_t) configured_ports->size(), "System MIDI In", AAP_CONTENT_TYPE_MIDI2,
                                AAP_PORT_DIRECTION_INPUT};
        configured_ports->emplace_back(midi_in);
    }
    if (!hasMidiOut) {
        PortInformation midi_out{(uint32_t) configured_ports->size(), "System MIDI Out",
                                 AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_OUTPUT};
        configured_ports->emplace_back(midi_out);
    }
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
    if (cached_parameters) {
        AAP_ASSERT_FALSE;
        return;
    }

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

void aap::PluginInstance::rebuildParameterIndexAndValues() {
    auto* state = get_parameter_state(this);
    if (!state)
        return;
    std::unordered_map<int32_t, double> previousValues;
    previousValues.reserve(state->values.size());
    for (auto& entry : state->id_to_index) {
        auto index = entry.second;
        if (index >= 0 && index < state->values.size())
            previousValues[entry.first] = state->values[index];
    }

    cached_parameters.reset();
    scanParametersAndBuildList();

    rebuild_parameter_id_index(this, state->id_to_index);

    std::vector<double> newValues;
    newValues.reserve(getNumParameters());
    for (int32_t i = 0, n = getNumParameters(); i < n; ++i) {
        auto* parameter = getParameter(i);
        auto it = previousValues.find(parameter->getId());
        if (it != previousValues.end())
            newValues.emplace_back(it->second);
        else
            newValues.emplace_back(parameter->getDefaultValue());
    }

    const std::lock_guard<NanoSleepLock> lock{state->mutex};
    state->values = std::move(newValues);
}

bool aap::PluginInstance::updateCachedParameterValueById(int32_t parameterId, double plainValue) {
    auto* state = get_parameter_state(this, false);
    if (!state)
        return false;
    auto it = state->id_to_index.find(parameterId);
    if (it == state->id_to_index.end())
        return false;
    auto index = it->second;
    if (index < 0 || index >= state->values.size())
        return false;
    state->values[index] = plainValue;
    return true;
}

void aap::PluginInstance::updateParameterValueCacheFromOutputBuffer(void* buffer) {
    if (!buffer)
        return;
    auto* state = get_parameter_state(this);
    if (!state)
        return;
    if (std::unique_lock<NanoSleepLock> tryLock(state->mutex, std::try_to_lock); !tryLock.owns_lock())
        return;

    if (state->id_to_index.empty())
        rebuild_parameter_id_index(this, state->id_to_index);
    if (state->values.empty()) {
        state->values.reserve(getNumParameters());
        for (int32_t i = 0, n = getNumParameters(); i < n; ++i) {
            auto* parameter = getParameter(i);
            state->values.emplace_back(parameter ? parameter->getDefaultValue() : 0.0);
        }
    }

    auto* mbh = (AAPMidiBufferHeader*) buffer;
    auto* data = (uint8_t*) (mbh + 1);
    uint32_t offset = 0;
    bool changed = false;

    while (offset + sizeof(uint32_t) <= mbh->length) {
        auto* ump = (uint32_t*) (data + offset);
        auto messageType = (*ump >> 28) & 0xF;
        auto messageSize = cmidi2_ump_get_message_size_bytes((cmidi2_ump*) ump);
        if (messageSize <= 0 || offset + static_cast<uint32_t>(messageSize) > mbh->length)
            break;

        if (messageType == 4 && messageSize >= 8) {
            auto word0 = ump[0];
            auto word1 = ump[1];
            auto status = (word0 >> 16) & 0xF0;
            auto channel = (word0 >> 16) & 0x0F;
            if (channel == 0) {
                int32_t parameterId = -1;
                switch (status) {
                    case 0x30:
                        parameterId = ((word0 >> 8) & 0x7F) << 7 | (word0 & 0x7F);
                        break;
                    case 0xB0:
                        parameterId = (word0 >> 8) & 0x7F;
                        break;
                    default:
                        break;
                }
                if (parameterId >= 0) {
                    auto it = state->id_to_index.find(parameterId);
                    if (it != state->id_to_index.end()) {
                        auto index = it->second;
                        if (index >= 0 && index < getNumParameters()) {
                            auto* parameter = getParameter(index);
                            auto plainValue = aapParameterTransportUint32ToPlain(
                                    parameter->getMinimumValue(),
                                    parameter->getMaximumValue(),
                                    word1);
                            changed |= updateCachedParameterValueById(parameterId, plainValue);
                        }
                    }
                }
            }
        } else if (messageType == 5 && messageSize >= 16) {
            auto word0 = ump[0];
            auto word1 = ump[1];
            auto word2 = ump[2];
            auto word3 = ump[3];
            if ((word0 & 0xFF) == 0x7E && (word1 >> 8) == 0x7F0000 && (word1 & 0x0F) == 0) {
                auto parameterId = static_cast<int32_t>(word2 & 0xFFFF);
                auto it = state->id_to_index.find(parameterId);
                if (it != state->id_to_index.end()) {
                    auto index = it->second;
                    if (index >= 0 && index < getNumParameters()) {
                        auto* parameter = getParameter(index);
                        auto plainValue = aapParameterTransportUint32ToPlain(
                                parameter->getMinimumValue(),
                                parameter->getMaximumValue(),
                                word3);
                        changed |= updateCachedParameterValueById(parameterId, plainValue);
                    }
                }
            }
        }

        offset += static_cast<uint32_t>(messageSize);
    }

    if (changed)
        state->revision.fetch_add(1);
}

double aap::PluginInstance::getParameterValue(int32_t index) {
    auto* state = get_parameter_state(const_cast<aap::PluginInstance*>(this), false);
    if (!state) {
        auto* parameter = getParameter(index);
        return parameter ? parameter->getDefaultValue() : 0.0;
    }
    const std::lock_guard<NanoSleepLock> lock{state->mutex};
    if (index >= 0 && index < state->values.size())
        return state->values[index];
    auto* parameter = getParameter(index);
    return parameter ? parameter->getDefaultValue() : 0.0;
}

uint32_t aap::PluginInstance::getParameterStateRevision() const {
    auto* state = get_parameter_state(const_cast<aap::PluginInstance*>(this), false);
    return state ? state->revision.load() : 1;
}

void aap::PluginInstance::handleParameterLayoutChanged() {
    rebuildParameterIndexAndValues();
    if (auto* state = get_parameter_state(this, false))
        state->revision.fetch_add(1);
}

aap_parameters_host_extension_t* aap::PluginInstance::getHostParametersExtension() {
    auto* state = get_parameter_state(this);
    if (state)
        state->host_parameters.notify_parameters_changed = notify_parameters_changed;
    return state ? &state->host_parameters : nullptr;
}

void aap::PluginInstance::activate() {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
        return;
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_INACTIVE) {
        AAP_ASSERT_FALSE;
        return;
    }

    plugin->activate(plugin);
    instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
}

void aap::PluginInstance::deactivate() {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE ||
        instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED)
        return;
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        AAP_ASSERT_FALSE;
        return;
    }

    plugin->deactivate(plugin);
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
}


void aap::PluginInstance::addEventUmpInput(void *input, int32_t size) {
    const std::lock_guard<NanoSleepLock> lock{ump_sequence_merger_mutex};
    if (event_midi2_buffer_offset + size > event_midi2_buffer_size) {
        aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG,
                     "Dropping %d-byte UMP input: pending queue overflow (%d + %d > %d)",
                     size,
                     event_midi2_buffer_offset,
                     size,
                     event_midi2_buffer_size);
        return;
    }
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
            auto mbh = (AAPMidiBufferHeader*) buffer->get_buffer(buffer, i);
            auto portBufferSize = buffer->get_buffer_size(buffer, i);
            auto midiCapacity = portBufferSize > static_cast<int32_t>(sizeof(AAPMidiBufferHeader)) ?
                    portBufferSize - static_cast<int32_t>(sizeof(AAPMidiBufferHeader)) : 0;
            auto mergeCapacity = std::min(mergeBufSize, midiCapacity);
            if (mergeCapacity <= 0) {
                aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG,
                             "Dropping merged MIDI input: destination port %d has no payload capacity.",
                             i);
                mbh->length = 0;
                return;
            }
            size_t newSize = cmidi2_ump_merge_sequences((cmidi2_ump*) mergeTmp, mergeCapacity,
                                                        (cmidi2_ump*) sequence, (size_t) sequenceSize,
                                                        (cmidi2_ump*) (mbh + 1), (size_t) mbh->length);
            if (newSize == static_cast<size_t>(mergeCapacity) &&
                (sequenceSize > 0 || mbh->length > 0))
                aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG,
                             "Merged MIDI input for port %d reached payload capacity (%d bytes). Input may be truncated.",
                             i,
                             mergeCapacity);
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
    auto para = ((aap::PluginInstance*) plugin->context)->getParameter(index);
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

void aap::PluginInstance::notify_parameters_changed(aap_parameters_host_extension_t* ext,
                                                    AndroidAudioPluginHost* host) {
    auto* instance = (PluginInstance*) host->context;
    instance->handleParameterLayoutChanged();
}

uint32_t aapxs_request_id_serial{0};
uint32_t aap::PluginInstance::aapxsRequestIdSerial() {
    return aapxs_request_id_serial++;
}

// AAPXS (v2 too)

void aap::PluginInstance::aapxsSessionAddEventUmpInput(aap::AAPXSMidi2InitiatorSession* client, void* context, int32_t messageSize) {
    auto instance = (aap::RemotePluginInstance *) context;
    instance->addEventUmpInput(client->aapxs_rt_midi_buffer, messageSize);
}
