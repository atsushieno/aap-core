
#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"
#include "plugin-parameter-state.h"
#include "parameter-layout-reader.h"
#include "async-parameter-layout.h"
#include "aapxs-transport.h"
#include "aapxs-shared-transport.h"
#include "instance-realtime-state.h"
#include "parameter-value-cache.h"
#include "midi2-port-buffer.h"
#include "aap/ext/midi.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include "../include_cmidi2.h"

#define LOG_TAG "AAP.Instance"

namespace {

// ---- Plugin-initiated parameter layout changes (see plugin-parameter-state.h)

struct ParameterLayoutState {
    std::shared_mutex list_mutex{};
    // Replaced lists stay alive until the instance is destroyed, so getParameter() pointers stay valid.
    std::vector<std::unique_ptr<std::vector<aap::ParameterInformation>>> retired_lists{};
    std::atomic<bool> ready{false};
    std::mutex listener_mutex{};
    std::function<void()> listener{};
};

std::mutex parameter_layout_state_registry_mutex;
std::unordered_map<aap::PluginInstance*, std::unique_ptr<ParameterLayoutState>> parameter_layout_state_registry;

ParameterLayoutState* get_parameter_layout_state(aap::PluginInstance* instance, bool create = true) {
    const std::lock_guard<std::mutex> lock{parameter_layout_state_registry_mutex};
    auto it = parameter_layout_state_registry.find(instance);
    if (it != parameter_layout_state_registry.end())
        return it->second.get();
    if (!create)
        return nullptr;
    auto state = std::make_unique<ParameterLayoutState>();
    auto* ret = state.get();
    parameter_layout_state_registry[instance] = std::move(state);
    return ret;
}

void refresh_parameter_layout(aap::PluginInstance* instance) {
    auto instanceState = instance->getInstanceState();
    if (instanceState == aap::PLUGIN_INSTANTIATION_STATE_INITIAL ||
        instanceState == aap::PLUGIN_INSTANTIATION_STATE_TERMINATED ||
        instanceState == aap::PLUGIN_INSTANTIATION_STATE_ERROR)
        return; // the host scans parameters right after instantiation anyway

    auto layout = get_parameter_layout_state(instance);
    auto remote = dynamic_cast<aap::RemotePluginInstance*>(instance);
    if (!remote && !layout->ready.load(std::memory_order_acquire))
        return;

    instance->scanParametersAndBuildList();

    if (remote) {
        if (remote->parametersChangedHandler)
            remote->parametersChangedHandler(*remote);
        std::function<void()> listener;
        {
            const std::lock_guard<std::mutex> lock{layout->listener_mutex};
            listener = layout->listener;
        }
        if (listener)
            listener();
    }
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
    parameter_values = std::make_unique<internal::ParameterValueCache>();
    host_plugin_info.get = get_plugin_info;
    if (!pluginInformation)
        AAP_ASSERT_FALSE; // should not happen
    if (!loadedPluginFactory)
        AAP_ASSERT_FALSE; // should not happen
    if (event_midi2_buffer_size <= 0)
        AAP_ASSERT_FALSE; // should not happen
    else {
        event_midi2_buffer = calloc(1, event_midi2_buffer_size);
        event_midi2_merge_buffer = calloc(1, event_midi2_buffer_size);
        realtime_state = std::make_unique<internal::InstanceRealtimeState>(event_midi2_buffer_size);
    }
    std::vector<internal::ParameterValueDescription> descriptions;
    for (int32_t i = 0; i < pluginInfo->getNumDeclaredParameters(); ++i) {
        auto* parameter = pluginInfo->getDeclaredParameter(i);
        descriptions.push_back({parameter->getId(), parameter->getMinimumValue(),
                parameter->getMaximumValue(), parameter->getDefaultValue()});
    }
    parameter_values->publish(descriptions);
}

static void publish_parameter_values(aap::PluginInstance& instance) {
    std::vector<aap::internal::ParameterValueDescription> descriptions;
    auto count = instance.getNumParameters();
    descriptions.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        auto* parameter = instance.getParameter(i);
        if (parameter) descriptions.push_back({parameter->getId(), parameter->getMinimumValue(),
                parameter->getMaximumValue(), parameter->getDefaultValue()});
    }
    instance.getParameterValueCache().publish(descriptions);
}

aap::PluginInstance::~PluginInstance() {
    stopExtensionWorker();
    internal::releaseAAPXSBinderChannels(this);
    instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
    releasePlugin();
    delete shared_memory_store;
    if (event_midi2_buffer)
        free(event_midi2_buffer);
    if (event_midi2_merge_buffer)
        free(event_midi2_merge_buffer);

    {
        const std::lock_guard<std::mutex> lock{parameter_layout_state_registry_mutex};
        parameter_layout_state_registry.erase(this);
    }
}

void aap::PluginInstance::releasePlugin() {
    instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
    if (plugin) {
        plugin_factory->release(plugin_factory, plugin);
        plugin = nullptr;
    }
}

aap_buffer_t* aap::PluginInstance::getAudioPluginBuffer() {
    return shared_memory_store ? shared_memory_store->getAudioPluginBuffer() : nullptr;
}

void aap::PluginInstance::completeInstantiation()
{
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_INITIAL) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to completeInstantiation() at state: %d (instanceId: %d)",
                     instantiation_state.load(), instance_id);
        return;
    }

    AndroidAudioPluginHost* asPluginAPI = getHostFacadeForCompleteInstantiation();
    plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), asPluginAPI);
    if (plugin) {
        instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
        realtime_state->worker.notify();
    } else {
        aap::a_log(AAP_LOG_LEVEL_WARN, LOG_TAG, "Plugin factory could not create an instance");
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
    if (RealtimeScope::isActive()) {
        internal::requestParameterLayoutRefresh(*this);
        return;
    }
    std::optional<internal::ProcessingQuiescence::Control> suspension;
    if (dynamic_cast<LocalPluginInstance*>(this)) suspension.emplace(realtime_state->processing);
    const std::lock_guard<std::mutex> scanLock{realtime_state->parameter_scan_mutex};
    internal::ParameterLayoutReader reader{getStandardExtensions()};
    auto result = internal::readParameterLayout(reader);
    if (!result.isOk()) {
        if (result.error != "parameters extension unavailable")
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Parameter scan aborted: %s", result.error.c_str());
        return;
    }
    auto scannedParameters = std::make_unique<std::vector<ParameterInformation>>(std::move(result.value));
    if (auto* local = dynamic_cast<LocalPluginInstance*>(this)) local->refreshExtensionState();

    // Publish immutable metadata and value-index snapshots. Processing holds neither lock.
    // The old list is retired, not freed, so that getParameter() pointers stay valid.
    auto* layout = get_parameter_layout_state(this);
    const std::unique_lock<std::shared_mutex> listLock{layout->list_mutex};
    if (cached_parameters)
        layout->retired_lists.emplace_back(std::move(cached_parameters));
    cached_parameters = std::move(scannedParameters);
    published_parameters.store(cached_parameters.get(), std::memory_order_release);
    publish_parameter_values(*this);
}

void aap::internal::cleanupParameterState(aap::PluginInstance&) {
    // State is now owned directly by the instance, released after its worker has joined.
}

void aap::internal::reindexParameterValues(aap::PluginInstance& instance) {
    const std::lock_guard<std::mutex> writers{instance.getRealtimeState().parameter_scan_mutex};
    publish_parameter_values(instance);
}

void aap::internal::rebuildParameterIndexAndValues(aap::PluginInstance& instance) {
    // scanParametersAndBuildList() reindexes the values when it publishes the new list.
    instance.scanParametersAndBuildList();
}

bool aap::internal::updateCachedParameterValueById(aap::PluginInstance& instance, int32_t parameterId, double plainValue) {
    return instance.getParameterValueCache().setById(parameterId, plainValue);
}

namespace {
void updateParameterValueCacheFromBuffer(aap::PluginInstance& instance, void* buffer, bool acceptChannelVoiceMessages);
}

void aap::internal::updateParameterValueCacheFromOutputBuffer(aap::PluginInstance& instance, void* buffer) {
    updateParameterValueCacheFromBuffer(instance, buffer, true);
}

void aap::internal::updateParameterValueCacheFromInputBuffer(aap::PluginInstance& instance, void* buffer) {
    // On the input side, CC / assignable controllers that were not translated to AAP parameter
    // SysEx8 (by the MIDI mapping policy) are meant for the plugin as plain MIDI, so only the
    // SysEx8 form counts as a parameter change.
    updateParameterValueCacheFromBuffer(instance, buffer, false);
}

namespace {
void updateParameterValueCacheFromBuffer(aap::PluginInstance& instance, void* buffer, bool acceptChannelVoiceMessages) {
    using namespace aap;
    using namespace aap::internal;
    if (!buffer)
        return;
    auto* state = instance.getParameterValueCache().snapshot();
    if (!state) return;

    auto* mbh = (AAPMidiBufferHeader*) buffer;
    auto* data = (uint8_t*) (mbh + 1);
    uint32_t offset = 0;
    while (offset + sizeof(uint32_t) <= mbh->length) {
        auto* ump = (uint32_t*) (data + offset);
        auto messageType = (*ump >> 28) & 0xF;
        auto messageSize = cmidi2_ump_get_message_size_bytes((cmidi2_ump*) ump);
        if (messageSize <= 0 || offset + static_cast<uint32_t>(messageSize) > mbh->length)
            break;

        if (acceptChannelVoiceMessages && messageType == 4 && messageSize >= 8) {
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
                        if (index < state->entries.size()) {
                            const auto& parameter = state->entries[index].description;
                            auto plainValue = aapParameterTransportUint32ToPlain(
                                    parameter.minimum,
                                    parameter.maximum,
                                    word1);
                            state->setById(parameterId, plainValue);
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
                    if (index < state->entries.size()) {
                        const auto& parameter = state->entries[index].description;
                        auto plainValue = aapParameterTransportUint32ToPlain(
                                parameter.minimum,
                                parameter.maximum,
                                word3);
                        state->setById(parameterId, plainValue);
                    }
                }
            }
        }

        offset += static_cast<uint32_t>(messageSize);
    }

}
}

double aap::internal::getParameterValue(aap::PluginInstance& instance, int32_t index) {
    return instance.getParameterValueCache().getByIndex(index);
}

void aap::internal::setCachedParameterValue(aap::PluginInstance& instance, int32_t index, double plainValue) {
    instance.getParameterValueCache().setByIndex(index, plainValue);
}

void aap::internal::handleParameterLayoutChanged(aap::PluginInstance& instance) {
    // A plugin can notify a parameter-layout change from within its own instantiate()
    // (e.g. JUCE/Dexed populate parameters during construction), which arrives before
    // completeInstantiation() has assigned `plugin`. Driving the parameter scan here
    // would dereference a null `plugin`. The instance is not configured yet, so there is
    // nothing valid to scan; the host performs the definitive parameter scan immediately
    // after instantiation completes (see PluginHost.Client createInstance:
    // scanParametersAndBuildList() + handleParameterLayoutChanged()), which reflects this
    // notification. Until then, ignore layout-change notifications.
    if (instance.getInstanceState() == PLUGIN_INSTANTIATION_STATE_INITIAL)
        return;
    rebuildParameterIndexAndValues(instance);
}

void aap::PluginInstance::activate() {
    if (RealtimeScope::isActive()) return;
    std::optional<internal::ProcessingQuiescence::Control> suspension;
    if (dynamic_cast<LocalPluginInstance*>(this)) suspension.emplace(realtime_state->processing);
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
    if (RealtimeScope::isActive()) return;
    std::optional<internal::ProcessingQuiescence::Control> suspension;
    if (dynamic_cast<LocalPluginInstance*>(this)) suspension.emplace(realtime_state->processing);
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE ||
        instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED)
        return;
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        AAP_ASSERT_FALSE;
        return;
    }

    plugin->deactivate(plugin);
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
    if (dynamic_cast<LocalPluginInstance*>(this))
        realtime_state->reset_deferred_midi.store(true, std::memory_order_release);
    realtime_state->worker.notify();
}

void aap::PluginInstance::addEventUmpInput(void *input, int32_t size) {
    (void) tryAddEventUmpInput(input, size);
}

bool aap::PluginInstance::tryAddEventUmpInput(const void* input, int32_t size) {
    return size > 0 && size <= event_midi2_buffer_size &&
            internal::isCompleteUmpSequence(input, static_cast<size_t>(size)) &&
            realtime_state->ump_input.tryPush(input, static_cast<size_t>(size));
}

void aap::PluginInstance::startExtensionWorker() {
    realtime_state->worker.start([this] {
        pollExtensionWorker();
        if (!realtime_state->worker.isStopping()) pollParameterLayoutRefresh();
    }, [this] { return nextExtensionDeadline(); });
}
void aap::PluginInstance::stopExtensionWorker() {
    if (realtime_state) {
        auto legacyGate = realtime_state->legacy_sender.cancel("AAPXS instance stopped", plugin, true);
        realtime_state->recipient_requests.close();
        legacyGate.unlock();
        realtime_state->worker.stop();
    }
}
void aap::PluginInstance::requestExtensionWorkerStop() {
    if (realtime_state) realtime_state->worker.requestStop();
}
bool aap::PluginInstance::isOnExtensionWorkerThread() const {
    return realtime_state && realtime_state->worker.isCurrentThread();
}

void aap::PluginInstance::mergeQueuedUmp(aap_port_direction direction, bool output) {
    auto buffer = getAudioPluginBuffer();
    if (!buffer) return;
    auto& queue = output ? realtime_state->ump_output : realtime_state->ump_input;
    for (int i = 0; i < getNumPorts(); ++i) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 || port->getPortDirection() != direction) continue;
        auto header = internal::getMidi2PortBuffer(buffer, i);
        if (!header) return;
        auto capacity = std::min(event_midi2_buffer_size,
                std::max(0, buffer->get_buffer_size(buffer, i) - static_cast<int32_t>(sizeof(*header))));
        size_t used = 0;
        for (unsigned n = 0; n < 64; ++n) {
            if (!queue.tryConsume([&](void* data, size_t size) {
                if (size > static_cast<size_t>(capacity)) return true; // cannot ever fit this port
                if (size + used + header->length > static_cast<size_t>(capacity)) return false;
                memcpy(static_cast<uint8_t*>(event_midi2_buffer) + used, data, size);
                used += size;
                return true;
            })) break;
        }
        merge_ump_sequences(direction, event_midi2_merge_buffer, event_midi2_buffer_size,
                event_midi2_buffer, static_cast<int32_t>(used), buffer, this);
        return;
    }
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
                mbh->length = 0;
                return;
            }
            size_t newSize = cmidi2_ump_merge_sequences((cmidi2_ump*) mergeTmp, mergeCapacity,
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

std::atomic<uint32_t> aapxs_request_id_serial{0};
uint32_t aap::PluginInstance::aapxsRequestIdSerial() {
    return aapxs_request_id_serial.fetch_add(1);
}

// AAPXS (v2 too)

bool aap::PluginInstance::aapxsSessionAddEventUmpInput(aap::AAPXSMidi2InitiatorSession* client, void* context, int32_t messageSize) {
    auto instance = (aap::RemotePluginInstance *) context;
    return instance->tryAddEventUmpInput(client->aapxs_rt_midi_buffer, messageSize);
}

// ---- Plugin-initiated parameter layout changes (see plugin-parameter-state.h)

thread_local bool aap::internal::ScopedBinderOnlyAAPXS::active{false};

void aap::internal::requestParameterLayoutRefresh(aap::PluginInstance& instance) {
    instance.getRealtimeState().layout_refresh.store(true, std::memory_order_release);
    instance.getRealtimeState().worker.notify();
}

void aap::PluginInstance::pollParameterLayoutRefresh() {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INITIAL || !plugin) return;
    if (!realtime_state->layout_refresh.load(std::memory_order_acquire) && !realtime_state->layout_scan) return;
    // Preserve early notifications until service extensions are ready. Readiness
    // explicitly wakes the worker; there is no periodic retry to rely on.
    if (dynamic_cast<LocalPluginInstance*>(this) &&
        !get_parameter_layout_state(this)->ready.load(std::memory_order_acquire)) return;
    if (auto* remote = dynamic_cast<RemotePluginInstance*>(this)) {
        auto& scan = realtime_state->layout_scan;
        if (scan && realtime_state->layout_refresh.load(std::memory_order_acquire) && !scan->canSupersede())
            return; // the current read's completion wakes this worker
        if (realtime_state->layout_refresh.exchange(false, std::memory_order_acq_rel)) {
            // A newer preset/layout notification invalidates this scan. Do not
            // finish hundreds of obsolete reads before scanning the new layout.
            scan.reset();
            auto* proxy = getStandardExtensions().asParametersExtension();
            if (!proxy) return;
            auto* transport = static_cast<xs::ParametersClientAAPXS*>(proxy->aapxs_context);
            scan = std::make_shared<internal::AsyncParameterLayout>(*transport,
                    !(remote->getAAPXSDispatcher().getTransportCapabilities() & internal::AAPXS_TRANSPORT_SYSEX8),
                    [this] { realtime_state->worker.notify(); });
            scan->start();
        }
        if (!scan) return;
        const auto complete = scan->poll(&realtime_state->layout_refresh);
        if (realtime_state->layout_refresh.load(std::memory_order_acquire)) {
            if (scan->canSupersede()) realtime_state->worker.notify();
            return;
        }
        if (!complete) return;
        auto result = scan->takeResult();
        scan.reset();
        if (realtime_state->layout_refresh.load(std::memory_order_acquire)) realtime_state->worker.notify();
        if (!result.isOk()) {
            if (result.error != "parameters extension unavailable")
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Parameter scan aborted: %s", result.error.c_str());
            return;
        }
        {
            const std::lock_guard<std::mutex> scanLock{realtime_state->parameter_scan_mutex};
            auto* layout = get_parameter_layout_state(this);
            const std::unique_lock<std::shared_mutex> listLock{layout->list_mutex};
            if (cached_parameters) layout->retired_lists.emplace_back(std::move(cached_parameters));
            cached_parameters = std::make_unique<std::vector<ParameterInformation>>(std::move(result.value));
            published_parameters.store(cached_parameters.get(), std::memory_order_release);
            publish_parameter_values(*this);
        }
        auto* layout = get_parameter_layout_state(this);
        std::function<void()> listener;
        {
            const std::lock_guard<std::mutex> lock{layout->listener_mutex};
            listener = layout->listener;
        }
        if (remote->parametersChangedHandler) remote->parametersChangedHandler(*remote);
        if (listener) listener();
        return;
    }
    if (realtime_state->layout_refresh.exchange(false, std::memory_order_acq_rel))
        refresh_parameter_layout(this);
}

void aap::internal::setParameterLayoutRefreshReady(aap::PluginInstance& instance) {
    get_parameter_layout_state(&instance)->ready.store(true, std::memory_order_release);
    instance.getRealtimeState().worker.notify();
}

void aap::internal::setParameterLayoutChangedListener(aap::RemotePluginInstance& instance, std::function<void()> listener) {
    auto* layout = get_parameter_layout_state(&instance);
    const std::lock_guard<std::mutex> lock{layout->listener_mutex};
    layout->listener = std::move(listener);
}

void aap::internal::closeParameterLayoutRefresh(aap::PluginInstance& instance) {
    instance.stopExtensionWorker();
}

void aap::internal::forgetParameterLayoutRefresh(aap::PluginInstance&) {
    // No process-wide owner/address registry remains.
}

int32_t aap::internal::getParameterCountSafely(aap::PluginInstance& instance) {
    return instance.getNumParameters();
}

const aap::ParameterInformation* aap::internal::getParameterSafely(aap::PluginInstance& instance, int32_t index) {
    return instance.getParameter(index);
}
