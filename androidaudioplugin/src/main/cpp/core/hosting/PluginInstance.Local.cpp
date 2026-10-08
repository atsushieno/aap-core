#include "aap/core/host/shared-memory-store.h"
#include "aap/core/host/plugin-instance.h"
#include "plugin-parameter-state.h"
#include <unordered_map>
#include <vector>
#include "aapxs-transport.h"
#include "aapxs-shared-transport.h"
#include "midi2-port-buffer.h"
#include "instance-realtime-state.h"

#define LOG_TAG "AAP.Local.Instance"

int32_t readLocalGuiListenerMidi2Output(aap::LocalPluginInstance* instance, void* output, int32_t size) {
    if (!instance || !output || size <= 0 || aap::RealtimeScope::isActive()) return 0;
    auto& state = instance->getRealtimeState();
    const std::lock_guard<std::mutex> consumers{state.gui_read_mutex};
    size_t copied = 0;
    for (unsigned n = 0; n < 64 && copied < static_cast<size_t>(size); ++n) {
        if (!state.gui_output.tryConsume([&](void* data, size_t length) {
            auto bytes = std::min(length - state.gui_read_offset, static_cast<size_t>(size) - copied);
            memcpy(static_cast<uint8_t*>(output) + copied, static_cast<uint8_t*>(data) + state.gui_read_offset, bytes);
            copied += bytes;
            state.gui_read_offset += bytes;
            if (state.gui_read_offset < length) return false;
            state.gui_read_offset = 0;
            return true;
        })) break;
    }
    return static_cast<int32_t>(copied);
}

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
        int32_t eventMidi2InputBufferSize)
        : PluginInstance(pluginInformation, loadedPluginFactory, eventMidi2InputBufferSize),
          host(host),
          aapxs_host_session(eventMidi2InputBufferSize),
          feature_registry(new xs::AAPXSDefinitionServiceRegistry(aapxsRegistry)),
          aapxs_dispatcher(aapxsRegistry)
          {
    shared_memory_store = new aap::ServicePluginSharedMemoryStore();
    instance_id = instanceId;


    aapxs_midi2_in_session.setExtensionCallback([&](aap_midi2_aapxs_parse_context* context) {
        handleAAPXSInput(context);
    });
}

aap::LocalPluginInstance::~LocalPluginInstance() {
    stopExtensionWorker();
    releasePlugin();

}

AndroidAudioPluginHost* aap::LocalPluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = internalGetHostExtension;
    plugin_host_facade.request_process = internalRequestProcess;
    return &plugin_host_facade;
}

void *
aap::LocalPluginInstance::getHostExtension(uint8_t urid, const char *uri) {
    // Plugin info is exposed directly; the remaining proxies were cached at setup.
    if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
        return &host_plugin_info;
    }
    if (strcmp(uri, AAP_BUSES_EXTENSION_URI) == 0) {
        return &host_buses;
    }
    if (!urid) urid = getAAPXSRegistry()->items()->getUridMapping()->getUrid(uri);
    return host_extension_proxies[urid];
}

void aap::LocalPluginInstance::internalRequestProcess(AndroidAudioPluginHost *host) {
    auto instance = (LocalPluginInstance *) host->context;
    instance->requestProcessToHost();
}

void aap::LocalPluginInstance::confirmPorts() {
    // The client configures its ports from the same layout that we reported (bus mode), or
    // from the same metadata (legacy mode).
    if (isBusMode() && (reported_bus_layout->flags & AAP_BUSES_LAYOUT_PLUGIN_PROVIDED)) {
        setupPortsFromBusLayout(*reported_bus_layout);
        return;
    }
    if (pluginInfo->getNumDeclaredPorts() == 0)
        setupPortConfigDefaults();
    else
        setupPortsViaMetadata();
    rebuildBusesFromPorts();
}

bool aap::LocalPluginInstance::BusesService::commitBufferLayout(const aap_buffer_layout_t& layout) {
    // The layout must be based on the bus layout that we reported, and buffers change only
    // while the instance is not active.
    if (!owner->reported_bus_layout || layout.generation != owner->bus_layout_generation ||
        owner->instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE ||
        layout.entry_count < 0 || layout.entry_count > AAP_MAX_BUFFER_LAYOUT_ENTRIES)
        return false;
    owner->committed_buffer_layout = std::make_unique<aap_buffer_layout_t>(layout);
    return true;
}

void aap::LocalPluginInstance::notifyBusesChanged(aap_buses_host_extension_t* ext, AndroidAudioPluginHost* host, uint32_t flags) {
    auto self = (LocalPluginInstance*) ext->aapxs_context;
    // The current buffers stay until the host prepares again; a buffer layout for the former
    // bus layout is rejected from now on.
    if (flags & AAP_BUSES_CHANGED_LAYOUT)
        self->bus_layout_generation++;
    if (self->buses_host_proxy)
        self->buses_host_proxy->notify_buses_changed(self->buses_host_proxy, host, flags);
}

bool aap::LocalPluginInstance::BusesService::applyLayout(const aap_bus_layout_request_t& request) {
    auto state = owner->instantiation_state.load();
    // Buffers of a prepared instance can be replaced only with a pool.
    if (state == PLUGIN_INSTANTIATION_STATE_ACTIVE ||
        (state == PLUGIN_INSTANTIATION_STATE_INACTIVE && !owner->usesBufferPool()) ||
        request.count < 0 || request.count > AAP_MAX_BUSES)
        return false;
    auto plugin = owner->plugin;
    auto ext = (aap_buses_extension_t*) plugin->get_extension(plugin, AAP_BUSES_EXTENSION_URI);
    if (!ext || !ext->apply_layout)
        return false;
    // The current buffers do not match the new layout; process() does not touch them until
    // the instance is prepared again.
    const internal::ProcessingQuiescence::Control suspension{owner->realtime_state->processing};
    if (!ext->apply_layout(ext, plugin, &request))
        return false;
    owner->bus_layout_generation++;
    owner->instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
    return true;
}

std::string aap::LocalPluginInstance::beginPrepare() {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE) {
        if (!usesBufferPool())
            return "an instance can be prepared again only with a buffer pool";
        // From now on process() does not touch the port buffers until prepare() completes.
        const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
        instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
    } else if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED)
        return "unexpected state for prepare";
    confirmPorts();
    if (usesBufferPool() && committed_buffer_layout->entry_count != getNumPorts())
        return "the buffer layout does not match the ports";
    return {};
}

std::string aap::LocalPluginInstance::setupPortBuffers(int32_t frameCount) {
    auto shm = dynamic_cast<ServicePluginSharedMemoryStore*>(getSharedMemoryStore());
    if (!shm)
        return "unable to get shared memory extension";
    const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
    if (usesBufferPool()) {
        auto error = shm->completeServicePoolInitialization(*committed_buffer_layout, frameCount, *this);
        if (error.empty())
            aap::a_log_f(AAP_LOG_LEVEL_INFO, LOG_TAG, "Using a buffer pool of %u bytes for %d ports (instanceId: %d)",
                         committed_buffer_layout->pool_size, committed_buffer_layout->entry_count, instance_id);
        return error;
    }
    if (!shm->completeServiceInitialization(frameCount, *this, DEFAULT_CONTROL_BUFFER_SIZE))
        return "failed to allocate shared memory";
    return {};
}

void aap::LocalPluginInstance::BusesService::getBusLayoutSnapshot(aap_buses_layout_snapshot_t& snapshot) {
    auto plugin = owner->plugin;
    snapshot = {};
    snapshot.generation = owner->bus_layout_generation;
    auto ext = (aap_buses_extension_t*) plugin->get_extension(plugin, AAP_BUSES_EXTENSION_URI);
    if (ext && ext->get_bus_count && ext->get_bus) {
        snapshot.flags |= AAP_BUSES_LAYOUT_PLUGIN_PROVIDED;
        // Plugins report audio buses; the framework adds the main event buses.
        const int32_t maxAudioBuses = AAP_MAX_BUSES - 2;
        for (auto direction : {AAP_PORT_DIRECTION_INPUT, AAP_PORT_DIRECTION_OUTPUT}) {
            auto n = ext->get_bus_count(ext, plugin, AAP_BUS_KIND_AUDIO, direction);
            for (int32_t i = 0; i < n && snapshot.count < maxAudioBuses; i++) {
                auto bus = ext->get_bus(ext, plugin, AAP_BUS_KIND_AUDIO, direction, i);
                bus.kind = AAP_BUS_KIND_AUDIO;
                bus.direction = direction;
                bus.role = i == 0 ? AAP_BUS_ROLE_MAIN : AAP_BUS_ROLE_AUX;
                bus.name[AAP_MAX_BUS_NAME_CHARS - 1] = 0;
                bus.layout[AAP_MAX_BUS_LAYOUT_CHARS - 1] = 0;
                bus.channel_count = std::max(0, bus.channel_count);
                if (!bus.layout[0])
                    strncpy(bus.layout, BusInformation::getDefaultLayoutForChannelCount(bus.channel_count).c_str(),
                            AAP_MAX_BUS_LAYOUT_CHARS - 1);
                snapshot.buses[snapshot.count++] = bus;
            }
        }
        for (auto direction : {AAP_PORT_DIRECTION_INPUT, AAP_PORT_DIRECTION_OUTPUT}) {
            aap_bus_info_t bus{};
            bus.id = direction == AAP_PORT_DIRECTION_INPUT ? AAP_BUS_ID_MAIN_EVENT_INPUT : AAP_BUS_ID_MAIN_EVENT_OUTPUT;
            bus.kind = AAP_BUS_KIND_EVENT;
            bus.direction = direction;
            bus.role = AAP_BUS_ROLE_MAIN;
            strncpy(bus.name, direction == AAP_PORT_DIRECTION_INPUT ? "Event In" : "Event Out", AAP_MAX_BUS_NAME_CHARS - 1);
            bus.enabled = true;
            snapshot.buses[snapshot.count++] = bus;
        }
    }
    // Without the plugin's buses extension, the client derives the buses from the legacy port
    // configuration just like confirmPorts() does, so we report nothing else.
    owner->reported_bus_layout = std::make_unique<aap_buses_layout_snapshot_t>(snapshot);
}

void aap::LocalPluginInstance::requestProcessToHost() {
    if (!process_requested_to_host.exchange(true, std::memory_order_relaxed)) {
        realtime_state->process_notification.store(true, std::memory_order_release);
        realtime_state->worker.notify();
    }
}

void aap::LocalPluginInstance::addEventUmpOutput(void* input, int32_t size) {
    if (size > 0 && size <= event_midi2_buffer_size && internal::isCompleteUmpSequence(input, size))
        realtime_state->ump_output.tryPush(input, static_cast<size_t>(size));
}

void aap::LocalPluginInstance::prepare(int32_t maximumExpectedSamplesPerBlock, int32_t sampleRate) {
    (void) maximumExpectedSamplesPerBlock;
    if (RealtimeScope::isActive()) return;
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED &&
        instantiation_state != PLUGIN_INSTANTIATION_STATE_INACTIVE) {
        AAP_ASSERT_FALSE;
        return;
    }
    const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
    // As for shared port-buffer allocation, prepare requires processing stopped.
    // Retention is allocated from actual mapped capacities, never lazily by DSP.
    auto buffer = getAudioPluginBuffer();
    auto& deferred = realtime_state->deferred_midi;
    deferred.clear();
    deferred.resize(getNumPorts());
    for (int i = 0; i < getNumPorts(); ++i) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 || port->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
            continue;
        auto size = buffer->get_buffer_size(buffer, i);
        auto capacity = size > static_cast<int32_t>(sizeof(AAPMidiBufferHeader)) ? size - sizeof(AAPMidiBufferHeader) : 0;
        deferred[i] = std::make_unique<internal::DeferredMidiInput>(capacity);
    }
    sample_rate = sampleRate;
    plugin->prepare(plugin, sampleRate, buffer);
    refreshExtensionState();
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
    realtime_state->worker.notify();
}

void aap::LocalPluginInstance::refreshExtensionState() {
    if (!plugin || !aapxs_dispatcher.hasInstances()) return;
    const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
    for (auto& definition : *getAAPXSRegistry()->items())
        if (definition.uri && definition.on_plugin_state_changed)
            definition.on_plugin_state_changed(&definition, aapxs_dispatcher.getPluginAAPXSByUri(definition.uri), plugin);
    realtime_state->extension_state_ready.store(true, std::memory_order_release);
}

void aap::LocalPluginInstance::pollExtensionWorker() {
    if (realtime_state->process_notification.exchange(false, std::memory_order_acq_rel))
        ((PluginService*) host)->requestProcessToHost(instance_id);
    auto send = [this](const char* uri, int32_t opcode, uint32_t requestId) {
        // Notifications share the reverse-direction channel with host queries.
        // Their empty request and URI must outlive asynchronous Binder delivery.
        struct Notification {
            std::string uri;
            AAPXSSerializationContext empty{};
            static void completed(void* context, void*) { delete static_cast<Notification*>(context); }
            static void failed(void* context, void* hostContext, const char*) { completed(context, hostContext); }
        };
        auto pending = new Notification{uri};
        AAPXSRequestContext request{Notification::completed, pending, &pending->empty, 0,
            pending->uri.c_str(), requestId, opcode, Notification::failed};
        if (!sendHostAAPXSRequest(&request)) delete pending;
    };
    if (realtime_state->extension_state_ready.load(std::memory_order_acquire)) {
        std::array<uint32_t, 256> notifications{};
        bool changed = false;
        for (unsigned urid = 1; urid < notifications.size(); ++urid) {
            notifications[urid] = realtime_state->coalesced_notifications[urid].exchange(0, std::memory_order_acq_rel);
            if (!notifications[urid]) continue;
            auto* definition = aapxs_dispatcher.getDefinitionByUrid(urid);
            if (!definition || !definition->get_request_flags) continue;
            for (unsigned bit = 0; bit < 32; ++bit)
                if (notifications[urid] & (1u << bit))
                    changed |= (definition->get_request_flags(definition, true, -1 - static_cast<int32_t>(bit)) & AAPXS_REQUEST_STATE_CHANGED) != 0;
        }
        if (changed) refreshExtensionState();
        for (unsigned urid = 1; urid < notifications.size(); ++urid) {
            if (!notifications[urid]) continue;
            auto* definition = aapxs_dispatcher.getDefinitionByUrid(urid);
            if (!definition) continue;
            for (unsigned bit = 0; bit < 32; ++bit)
                if (notifications[urid] & (1u << bit)) send(definition->uri, -1 - static_cast<int32_t>(bit), aapxsRequestIdSerial());
        }
    }
    for (unsigned n = 0; n < 64 && !realtime_state->worker.isStopping(); ++n) {
        if (!realtime_state->extension_state_ready.load(std::memory_order_acquire)) break;
        if (!realtime_state->host_notifications.tryConsume([&](void* data, size_t) {
            auto& notification = *static_cast<internal::HostNotification*>(data);
            auto* definition = aapxs_dispatcher.getDefinitionByUri(notification.uri);
            if (definition && definition->get_request_flags &&
                (definition->get_request_flags(definition, true, notification.opcode) & AAPXS_REQUEST_STATE_CHANGED)) refreshExtensionState();
            send(notification.uri, notification.opcode, notification.request_id);
            return true;
        })) break;
    }
    for (unsigned n = 0; n < 64 && !realtime_state->worker.isStopping(); ++n) {
        if (!realtime_state->aapxs_input.tryConsume([this](void* data, size_t) {
            aapxs_midi2_in_session.process(data);
            return true;
        })) break;
    }
}

const char* local_trace_name = "AAP::LocalPluginInstance_process";
void aap::LocalPluginInstance::process(int32_t frameCount, int32_t timeoutInNanoseconds) {
    // There are no (stable) port buffers before prepare() completes, including while the
    // buffer pool is being replaced.
    auto state = instantiation_state.load();
    if (state != PLUGIN_INSTANTIATION_STATE_ACTIVE && state != PLUGIN_INSTANTIATION_STATE_INACTIVE)
        return;
    RealtimeScope realtime;
    internal::ProcessingQuiescence::Process activity(realtime_state->processing);
    if (realtime_state->reset_deferred_midi.exchange(false, std::memory_order_acq_rel))
        for (auto& input : realtime_state->deferred_midi) if (input) input->reset();
    if (!activity || instantiation_state != PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // Unsafe control work runs between blocks. Handoff extension requests,
        // retain ordinary MIDI per port, and emit silence without waiting.
        if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) mergeQueuedUmp(AAP_PORT_DIRECTION_INPUT);
        auto buffer = getAudioPluginBuffer();
        for (int i = 0; i < getNumPorts(); ++i) {
            auto port = getPort(i);
            if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2) {
                auto header = internal::getMidi2PortBuffer(buffer, i);
                if (!header) continue;
                if (port->getPortDirection() == AAP_PORT_DIRECTION_INPUT) {
                    internal::sysex8::filterOutMessages(header, realtime_state.get(), internal::queueAAPXSMidi2Input);
                    if (i < static_cast<int>(realtime_state->deferred_midi.size()) && realtime_state->deferred_midi[i]) {
                        auto& input = *realtime_state->deferred_midi[i];
                        if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) input.capture(*header);
                        else input.reset();
                    }
                }
                header->length = 0;
            } else if (port->getPortDirection() == AAP_PORT_DIRECTION_OUTPUT) {
                auto data = buffer->get_buffer(buffer, i);
                auto size = buffer->get_buffer_size(buffer, i);
                if (data && size > 0) memset(data, 0, size);
            }
        }
        return;
    }
    process_requested_to_host = false;

    struct timespec timeSpecBegin{}, timeSpecEnd{};
#if ANDROID
    if (ATrace_isEnabled()) {
        ATrace_beginSection(local_trace_name);
        clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
    }
#endif

    mergeQueuedUmp(AAP_PORT_DIRECTION_INPUT);

    // Copy AAPXS SysEx8 requests for extension-worker dispatch.
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
            continue;
        void *data = internal::getMidi2PortBuffer(getAudioPluginBuffer(), i);
        internal::sysex8::filterOutMessages(data, realtime_state.get(), internal::queueAAPXSMidi2Input);
        auto* header = static_cast<AAPMidiBufferHeader*>(data);
        if (header && i < static_cast<int>(realtime_state->deferred_midi.size()) && realtime_state->deferred_midi[i]) {
            auto& input = *realtime_state->deferred_midi[i];
            if (input.pending()) {
                input.age();
                input.capture(*header, false);
                auto size = getAudioPluginBuffer()->get_buffer_size(getAudioPluginBuffer(), i);
                input.drain(*header, size > static_cast<int32_t>(sizeof(*header)) ? size - sizeof(*header) : 0);
            }
            input.observe(*header);
        }
    }

    plugin->process(plugin, getAudioPluginBuffer(), frameCount, timeoutInNanoseconds);

    for (int i = 0; i < getNumPorts(); ++i) {
        auto port = getPort(i);
        if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2 && port->getPortDirection() == AAP_PORT_DIRECTION_INPUT) {
            auto* header = internal::getMidi2PortBuffer(getAudioPluginBuffer(), i);
            if (header) header->length = 0;
        }
    }

    // The plugin may have written a broken output length.
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2 && port->getPortDirection() == AAP_PORT_DIRECTION_OUTPUT)
            internal::getMidi2PortBuffer(getAudioPluginBuffer(), i);
    }

    // before sending back to host, merge AAPXS SysEx8 UMPs from async extension calls
    // into the plugin's MIDI output buffer.
    mergeQueuedUmp(AAP_PORT_DIRECTION_OUTPUT, true);

    auto aapBuffer = getAudioPluginBuffer();
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_OUTPUT)
            continue;
        auto* data = (AAPMidiBufferHeader*) aapBuffer->get_buffer(aapBuffer, i);
        internal::updateParameterValueCacheFromOutputBuffer(*this, data);
        if (data && data->length > 0)
            realtime_state->gui_output.tryPush(data + 1, data->length);
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
    const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
    standards = std::make_unique<xs::ServiceStandardExtensions>(plugin);
    refreshExtensionState();
    internal::setParameterLayoutRefreshReady(*this);
}

static inline void staticSendAAPXSReply(AAPXSRecipientInstance* instance, AAPXSRequestContext* context) {
    ((aap::LocalPluginInstance *) instance->host_context)->sendPluginAAPXSReply(context);
}
static inline bool staticSendAAPXSRequest(AAPXSInitiatorInstance* instance, AAPXSRequestContext* context) {
    return ((aap::LocalPluginInstance*) instance->host_context)->sendHostAAPXSRequest(context);
}

bool aap::LocalPluginInstance::setupAAPXSInstances() {
    auto store = getSharedMemoryStore();
    auto func = [&](const char* uri, AAPXSSerializationContext* serialization) {
        auto found = store->getExtensionUriToIndexMap().find(uri);
        if (found == store->getExtensionUriToIndexMap().end()) return;
        auto index = found->second;
        serialization->data = store->getExtensionBuffer(index);
        serialization->data_capacity = store->getExtensionBufferCapacity(index);
    };
    if (!aapxs_dispatcher.setupInstances(this,
                                    func,
                                    staticSendAAPXSReply,
                                    staticSendAAPXSRequest,
                                    staticGetNewRequestId,
                                    [this](auto& initiator) {
                                        initiator.plugin_id = pluginInfo->getPluginID().c_str();
                                        initiator.request_metadata_refresh = [](auto* instance) {
                                            internal::requestParameterLayoutRefresh(*static_cast<LocalPluginInstance*>(instance->host_context));
                                        };
                                    },
                                    [this](auto& recipient) { recipient.plugin_id = pluginInfo->getPluginID().c_str(); })) return false;
    realtime_state->recipient_requests.setSender([this](const AAPXSRequestContext& request) {
        // Separate scratch storage from the recipient parser: deferred replies may
        // publish concurrently with worker parsing and use the original route even
        // when processing has since stopped. All encoding is off the DSP thread.
        uint32_t wire[2 * AAP_MIDI2_AAPXS_DATA_MAX_SIZE / sizeof(uint32_t)];
        uint8_t scratch[AAP_MIDI2_AAPXS_DATA_MAX_SIZE];
        auto size = aap_midi2_generate_aapxs_sysex8(wire, std::size(wire), scratch, sizeof(scratch),
                0, request.request_id, request.urid, request.uri, request.opcode,
                static_cast<const uint8_t*>(request.serialization->data), request.serialization->data_size);
        return size && realtime_state->ump_output.tryPush(wire, size);
    });
    for (auto& definition : *getAAPXSRegistry()->items()) {
        if (!definition.uri || !definition.get_host_extension_proxy) continue;
        auto initiator = aapxs_dispatcher.getHostAAPXSByUri(definition.uri);
        auto proxy = definition.get_host_extension_proxy(&definition, initiator, initiator->serialization);
        auto urid = getAAPXSRegistry()->items()->getUridMapping()->getUrid(definition.uri);
        host_extension_proxies[urid] = proxy.as_host_extension ? proxy.as_host_extension(&proxy) : nullptr;
    }
    buses_host_proxy = (aap_buses_host_extension_t*) host_extension_proxies[
            getAAPXSRegistry()->items()->getUridMapping()->getUrid(AAP_BUSES_EXTENSION_URI)];
    startExtensionWorker();
    return true;
}

void
aap::LocalPluginInstance::sendPluginAAPXSReply(AAPXSRequestContext* request) {
    if (auto reply = xs::retainAAPXSReply(request)) reply->complete();
    // Borrowed Binder requests complete synchronously when extension() returns.
}

bool
aap::LocalPluginInstance::sendHostAAPXSRequest(AAPXSRequestContext* request) {
    if (realtime_state->worker.isStopping()) return false;
    auto* definition = request->urid ? aapxs_dispatcher.getDefinitionByUrid(request->urid) :
            (request->uri ? aapxs_dispatcher.getDefinitionByUri(request->uri) : nullptr);
    const auto flags = definition && definition->get_request_flags ?
            definition->get_request_flags(definition, true, request->opcode) : 0u;
    bool hasPayload = request->serialization && request->serialization->data_size > 0;
    if (!request->callback && !request->error_callback && !hasPayload && request->uri) {
        auto urid = getAAPXSRegistry()->items()->getUridMapping()->getUrid(request->uri);
        if ((flags & AAPXS_REQUEST_COALESCE) && urid && request->opcode >= -32 && request->opcode <= -1) {
            realtime_state->coalesced_notifications[urid].fetch_or(1u << (-1 - request->opcode), std::memory_order_release);
            realtime_state->worker.notify();
        } else {
            internal::HostNotification notification{};
            auto length = strnlen(request->uri, sizeof(notification.uri));
            if (length == sizeof(notification.uri)) return false;
            memcpy(notification.uri, request->uri, length + 1);
            notification.opcode = request->opcode;
            notification.request_id = request->request_id;
            if (realtime_state->host_notifications.tryPush(&notification, sizeof(notification))) realtime_state->worker.notify();
        }
        return false;
    }
    if (RealtimeScope::isActive()) return false;
    if ((flags & AAPXS_REQUEST_STATE_CHANGED) && !realtime_state->worker.isCurrentThread()) refreshExtensionState();

    auto& dispatcher = getAAPXSDispatcher();
    auto aapxsInstance = request->urid != 0 ? dispatcher.getHostAAPXSByUrid(request->urid) : dispatcher.getHostAAPXSByUri(request->uri);
    if (!aapxsInstance || !aapxsInstance->serialization)
        return false;
    if (definition && definition->on_outgoing_host_request)
        definition->on_outgoing_host_request(definition, aapxsInstance, request);
    auto channel = internal::getAAPXSBinderChannel(this, aapxsInstance->serialization, [this] {
        return [this](const AAPXSRequestContext& routed) {
            if (!ipc_send_extension_message_func) return false;
            getAAPXSDispatcher().publishBinderRequestSize(routed.serialization);
            ipc_send_extension_message_func(ipc_send_extension_message_context, routed.uri, instance_id,
                    routed.opcode, static_cast<int32_t>(routed.request_id), routed.callback,
                    routed.callback_user_data, &plugin_host_facade, routed.error_callback);
            return true;
        };
    }, [this, block = aapxsInstance->serialization]() -> std::optional<size_t> {
        if (!(getAAPXSDispatcher().getTransportCapabilities() & internal::AAPXS_TRANSPORT_LENGTHS)) return std::nullopt;
        return getAAPXSDispatcher().getBinderReplySize(block);
    });
    return channel->send(request);
}

void aap::LocalPluginInstance::controlExtension(uint8_t urid, const std::string &uri, int32_t opcode, uint32_t requestId)  {
    if (RealtimeScope::isActive()) return; // control dispatch belongs to Binder/extension workers
    auto registry = feature_registry->items();
    auto def = urid != 0 ? registry->getByUrid(urid) : registry->getByUri(uri.c_str());
    const auto flags = def && def->get_request_flags ? def->get_request_flags(def, false, opcode) : 0u;
    std::optional<internal::ProcessingQuiescence::Control> suspension;
    if (!(flags & AAPXS_REQUEST_CONCURRENT) || !realtime_state->extension_state_ready.load(std::memory_order_acquire))
        suspension.emplace(realtime_state->processing);
    // The registry returns an empty slot for an unknown URI. A client may know newer extensions,
    // so report it as an error instead of crashing.
    auto& dispatcher = getAAPXSDispatcher();
    auto instance = !def || !def->uri ? nullptr :
                    urid != 0 ? dispatcher.getPluginAAPXSByUrid(urid) : dispatcher.getPluginAAPXSByUri(uri.c_str());
    if (!instance || !def->process_incoming_plugin_aapxs_request)
        throw std::runtime_error("Unsupported extension: " + uri);
    dispatcher.receiveBinderRequest(instance->serialization);
    AAPXSRequestContext context{nullptr, nullptr, instance->serialization, urid, uri.c_str(), requestId, opcode};
    // Only the extension can declare its handler independent of plugin state.
    def->process_incoming_plugin_aapxs_request(def, instance, plugin, &context);
    if (!(flags & AAPXS_REQUEST_READ_ONLY)) refreshExtensionState();
    dispatcher.publishBinderReplySize(instance->serialization);
}

void aap::LocalPluginInstance::handleAAPXSInput(aap_midi2_aapxs_parse_context *context) {
    if (context->opcode >= 0) {
        auto registry = feature_registry->items();
        auto def = context->urid ? registry->getByUrid(context->urid) : registry->getByUri(context->uri);
        auto instance = context->urid ? aapxs_dispatcher.getPluginAAPXSByUrid(context->urid) :
                                      aapxs_dispatcher.getPluginAAPXSByUri(context->uri);
        if (!def || !instance || !def->process_incoming_plugin_aapxs_request) return;
        AAPXSSerializationContext buffer{context->data, context->dataSize, AAP_MIDI2_AAPXS_DATA_MAX_SIZE};
        AAPXSRequestContext incoming{nullptr, nullptr, &buffer, context->urid, def->uri, context->request_id, context->opcode};
        auto reply = realtime_state->recipient_requests.create(incoming, AAP_MIDI2_AAPXS_DATA_MAX_SIZE);
        if (!reply) return; // bounded rejection leaves the initiator's timeout intact
        const auto flags = def->get_request_flags ? def->get_request_flags(def, false, context->opcode) : 0u;
        std::optional<internal::ProcessingQuiescence::Control> suspension;
        if (!(flags & AAPXS_REQUEST_CONCURRENT) || !realtime_state->extension_state_ready.load(std::memory_order_acquire))
            suspension.emplace(realtime_state->processing);
        def->process_incoming_plugin_aapxs_request(def, instance, plugin, &reply->request());
        if (!(flags & AAPXS_REQUEST_READ_ONLY)) refreshExtensionState();
    } else {
        // host reply
        auto& dispatcher = getAAPXSDispatcher();
        auto aapxsInstance = context->urid != 0 ? dispatcher.getHostAAPXSByUrid(context->urid) : dispatcher.getHostAAPXSByUri(context->uri);
        // We need to copy extension data buffer before calling it.
        memcpy(aapxsInstance->serialization->data, (int32_t*) context->data, context->dataSize);
        aapxsInstance->serialization->data_size = context->dataSize;

        auto registry = feature_registry.get()->items();
        auto def = context->urid != 0 ? registry->getByUrid(context->urid) : registry->getByUri(context->uri);
        if (def) { // ignore undefined extensions here
            AAPXSRequestContext request{nullptr, nullptr, aapxsInstance->serialization,
                                        context->urid, context->uri, context->request_id, context->opcode};
            def->process_incoming_host_aapxs_reply(def, aapxsInstance, &plugin_host_facade, &request);
        }
    }
}
