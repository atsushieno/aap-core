
#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"
#include "plugin-parameter-state.h"
#include "parameter-layout-reader.h"
#include "aapxs-transport.h"
#include "aap/ext/midi.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include "../include_cmidi2.h"

#define LOG_TAG "AAP.Instance"

namespace {
struct PluginParameterState {
    aap::NanoSleepLock mutex{};
    std::vector<double> values{};
    std::unordered_map<int32_t, int32_t> id_to_index{};
};

std::mutex parameter_state_registry_mutex;
std::mutex parameter_layout_scan_mutex;
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

    {
        aap::internal::ScopedBinderOnlyAAPXS binderOnly;
        instance->scanParametersAndBuildList();
    }

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

// A process-wide worker that runs refresh_parameter_layout(). request() is RT-safe once started,
// and an instance that is already pending is not queued twice.
class ParameterLayoutRefreshQueue {
    aap::NanoSleepLock lock{};
    std::array<aap::PluginInstance*, 256> pending{};
    size_t pending_count{0};
    std::array<const aap::PluginInstance*, 64> closed{};
    aap::PluginInstance* refreshing{nullptr};
    sem_t available{};
    std::once_flag start_once{};
    std::atomic<bool> started{false};

    bool isPending(aap::PluginInstance* instance) const {
        return std::find(pending.begin(), pending.begin() + pending_count, instance) != pending.begin() + pending_count;
    }

    bool isClosed(const aap::PluginInstance* instance) const {
        return std::find(closed.begin(), closed.end(), instance) != closed.end();
    }

    void removePending(aap::PluginInstance* instance) {
        auto end = std::remove(pending.begin(), pending.begin() + pending_count, instance);
        pending_count = end - pending.begin();
    }

    aap::PluginInstance* takeNext() {
        const std::lock_guard<aap::NanoSleepLock> guard{lock};
        if (pending_count == 0)
            return nullptr;
        refreshing = pending[0];
        std::move(pending.begin() + 1, pending.begin() + pending_count, pending.begin());
        pending_count--;
        return refreshing;
    }

    void run() {
        pthread_setname_np(pthread_self(), "AAP.ParamLayout");
        while (true) {
            while (sem_wait(&available) != 0 && errno == EINTR) {}
            while (auto instance = takeNext()) {
                refresh_parameter_layout(instance);
                const std::lock_guard<aap::NanoSleepLock> guard{lock};
                refreshing = nullptr;
            }
        }
    }

public:
    // Not RT-safe.
    void start() {
        std::call_once(start_once, [this] {
            sem_init(&available, 0, 0);
            std::thread([this] { run(); }).detach();
            started.store(true, std::memory_order_release);
        });
    }

    void request(aap::PluginInstance* instance) {
        if (!started.load(std::memory_order_acquire))
            start();
        {
            const std::lock_guard<aap::NanoSleepLock> guard{lock};
            if (isClosed(instance) || isPending(instance) || pending_count == pending.size())
                return;
            pending[pending_count++] = instance;
        }
        sem_post(&available);
    }

    void close(aap::PluginInstance* instance) {
        while (true) {
            {
                const std::lock_guard<aap::NanoSleepLock> guard{lock};
                removePending(instance);
                if (!isClosed(instance)) {
                    auto slot = std::find(closed.begin(), closed.end(), nullptr);
                    if (slot != closed.end())
                        *slot = instance;
                }
                if (refreshing != instance)
                    return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void forget(aap::PluginInstance* instance) {
        const std::lock_guard<aap::NanoSleepLock> guard{lock};
        std::replace(closed.begin(), closed.end(), (const aap::PluginInstance*) instance, (const aap::PluginInstance*) nullptr);
    }
};

// Intentionally never destroyed: the worker runs for the process lifetime.
ParameterLayoutRefreshQueue& parameter_layout_refresh_queue() {
    static auto* queue = new ParameterLayoutRefreshQueue();
    return *queue;
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

// Rebuilds the id->index map and value vector for the *current* parameter list, preserving
// previously-known values by id. The caller must hold state.mutex.
static void reindex_parameter_values_locked(aap::PluginInstance& instance, PluginParameterState& state) {
    std::unordered_map<int32_t, double> previousValues;
    previousValues.reserve(state.values.size());
    for (auto& entry : state.id_to_index) {
        auto index = entry.second;
        if (index >= 0 && index < state.values.size())
            previousValues[entry.first] = state.values[index];
    }

    std::unordered_map<int32_t, int32_t> newIdToIndex;
    std::vector<double> newValues;
    newValues.reserve(instance.getNumParameters());
    for (int32_t i = 0, n = instance.getNumParameters(); i < n; ++i) {
        auto* parameter = instance.getParameter(i);
        if (!parameter)
            break;
        newIdToIndex[parameter->getId()] = i;
        auto it = previousValues.find(parameter->getId());
        newValues.emplace_back(it != previousValues.end() ? it->second : parameter->getDefaultValue());
    }

    state.id_to_index = std::move(newIdToIndex);
    state.values = std::move(newValues);
}

aap::PluginInstance::~PluginInstance() {
    internal::releaseAAPXSBinderChannels(this);
    instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
    if (plugin != nullptr)
        plugin_factory->release(plugin_factory, plugin);
    plugin = nullptr;
    delete shared_memory_store;
    if (event_midi2_buffer)
        free(event_midi2_buffer);
    if (event_midi2_merge_buffer)
        free(event_midi2_merge_buffer);
    internal::cleanupParameterState(*this);
    {
        const std::lock_guard<std::mutex> lock{parameter_layout_state_registry_mutex};
        parameter_layout_state_registry.erase(this);
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
                     instantiation_state, instance_id);
        return;
    }

    AndroidAudioPluginHost* asPluginAPI = getHostFacadeForCompleteInstantiation();
    plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), asPluginAPI);
    if (plugin) {
        instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
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
    const std::lock_guard<std::mutex> scanLock{parameter_layout_scan_mutex};
    internal::ParameterLayoutReader reader{getStandardExtensions()};
    auto result = internal::readParameterLayout(reader);
    if (!result.isOk()) {
        if (result.error != "parameters extension unavailable")
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "Parameter scan aborted: %s", result.error.c_str());
        return;
    }
    auto scannedParameters = std::make_unique<std::vector<ParameterInformation>>(std::move(result.value));

    // Publish and reindex under both the value lock (audio thread) and the list lock (safe readers).
    // The old list is retired, not freed, so that getParameter() pointers stay valid.
    auto* state = get_parameter_state(this);
    auto* layout = get_parameter_layout_state(this);
    const std::lock_guard<NanoSleepLock> valueLock{state->mutex};
    const std::unique_lock<std::shared_mutex> listLock{layout->list_mutex};
    if (cached_parameters)
        layout->retired_lists.emplace_back(std::move(cached_parameters));
    cached_parameters = std::move(scannedParameters);
    reindex_parameter_values_locked(*this, *state);
}

void aap::internal::cleanupParameterState(aap::PluginInstance& instance) {
    const std::lock_guard<std::mutex> lock{parameter_state_registry_mutex};
    parameter_state_registry.erase(&instance);
}

void aap::internal::reindexParameterValues(aap::PluginInstance& instance) {
    auto* state = get_parameter_state(&instance);
    if (!state)
        return;
    const std::lock_guard<NanoSleepLock> lock{state->mutex};
    reindex_parameter_values_locked(instance, *state);
}

void aap::internal::rebuildParameterIndexAndValues(aap::PluginInstance& instance) {
    // scanParametersAndBuildList() reindexes the values when it publishes the new list.
    instance.scanParametersAndBuildList();
}

bool aap::internal::updateCachedParameterValueById(aap::PluginInstance& instance, int32_t parameterId, double plainValue) {
    auto* state = get_parameter_state(&instance, false);
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
    auto* state = get_parameter_state(&instance);
    if (!state)
        return;
    if (std::unique_lock<NanoSleepLock> tryLock(state->mutex, std::try_to_lock); !tryLock.owns_lock())
        return;

    if (state->id_to_index.empty())
        rebuild_parameter_id_index(&instance, state->id_to_index);
    if (state->values.empty()) {
        state->values.reserve(instance.getNumParameters());
        for (int32_t i = 0, n = instance.getNumParameters(); i < n; ++i) {
            auto* parameter = instance.getParameter(i);
            state->values.emplace_back(parameter ? parameter->getDefaultValue() : 0.0);
        }
    }

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
                        if (index >= 0 && index < instance.getNumParameters()) {
                            auto* parameter = instance.getParameter(index);
                            auto plainValue = aapParameterTransportUint32ToPlain(
                                    parameter->getMinimumValue(),
                                    parameter->getMaximumValue(),
                                    word1);
                            updateCachedParameterValueById(instance, parameterId, plainValue);
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
                    if (index >= 0 && index < instance.getNumParameters()) {
                        auto* parameter = instance.getParameter(index);
                        auto plainValue = aapParameterTransportUint32ToPlain(
                                parameter->getMinimumValue(),
                                parameter->getMaximumValue(),
                                word3);
                        updateCachedParameterValueById(instance, parameterId, plainValue);
                    }
                }
            }
        }

        offset += static_cast<uint32_t>(messageSize);
    }

}
}

double aap::internal::getParameterValue(aap::PluginInstance& instance, int32_t index) {
    auto* state = get_parameter_state(&instance, false);
    if (!state) {
        auto* parameter = instance.getParameter(index);
        return parameter ? parameter->getDefaultValue() : 0.0;
    }
    const std::lock_guard<NanoSleepLock> lock{state->mutex};
    if (index >= 0 && index < state->values.size())
        return state->values[index];
    auto* parameter = instance.getParameter(index);
    return parameter ? parameter->getDefaultValue() : 0.0;
}

void aap::internal::setCachedParameterValue(aap::PluginInstance& instance, int32_t index, double plainValue) {
    auto* state = get_parameter_state(&instance);
    const std::lock_guard<NanoSleepLock> lock{state->mutex};
    if (index >= 0 && index < state->values.size())
        state->values[index] = plainValue;
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

std::atomic<uint32_t> aapxs_request_id_serial{0};
uint32_t aap::PluginInstance::aapxsRequestIdSerial() {
    return aapxs_request_id_serial.fetch_add(1);
}

// AAPXS (v2 too)

void aap::PluginInstance::aapxsSessionAddEventUmpInput(aap::AAPXSMidi2InitiatorSession* client, void* context, int32_t messageSize) {
    auto instance = (aap::RemotePluginInstance *) context;
    instance->addEventUmpInput(client->aapxs_rt_midi_buffer, messageSize);
}

// ---- Plugin-initiated parameter layout changes (see plugin-parameter-state.h)

thread_local bool aap::internal::ScopedBinderOnlyAAPXS::active{false};

void aap::internal::requestParameterLayoutRefresh(aap::PluginInstance& instance) {
    parameter_layout_refresh_queue().request(&instance);
}

void aap::internal::setParameterLayoutRefreshReady(aap::PluginInstance& instance) {
    get_parameter_layout_state(&instance)->ready.store(true, std::memory_order_release);
    parameter_layout_refresh_queue().start();
}

void aap::internal::setParameterLayoutChangedListener(aap::RemotePluginInstance& instance, std::function<void()> listener) {
    auto* layout = get_parameter_layout_state(&instance);
    const std::lock_guard<std::mutex> lock{layout->listener_mutex};
    layout->listener = std::move(listener);
}

void aap::internal::closeParameterLayoutRefresh(aap::PluginInstance& instance) {
    parameter_layout_refresh_queue().close(&instance);
}

void aap::internal::forgetParameterLayoutRefresh(aap::PluginInstance& instance) {
    parameter_layout_refresh_queue().forget(&instance);
}

int32_t aap::internal::getParameterCountSafely(aap::PluginInstance& instance) {
    auto* layout = get_parameter_layout_state(&instance);
    const std::shared_lock<std::shared_mutex> lock{layout->list_mutex};
    return instance.getNumParameters();
}

const aap::ParameterInformation* aap::internal::getParameterSafely(aap::PluginInstance& instance, int32_t index) {
    auto* layout = get_parameter_layout_state(&instance);
    const std::shared_lock<std::shared_mutex> lock{layout->list_mutex};
    if (index < 0 || index >= instance.getNumParameters())
        return nullptr;
    return instance.getParameter(index);
}
