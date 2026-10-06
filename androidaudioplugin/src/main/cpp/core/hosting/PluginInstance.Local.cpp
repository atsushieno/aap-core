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
    if (!urid) urid = getAAPXSRegistry()->items()->getUridMapping()->getUrid(uri);
    return host_extension_proxies[urid];
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
    refreshPollReplies();
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
    realtime_state->worker.notify();
}

void aap::LocalPluginInstance::refreshPollReplies() {
    auto parameters = static_cast<aap_parameters_extension_t*>(plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI));
    auto presets = static_cast<aap_presets_extension_t*>(plugin->get_extension(plugin, AAP_PRESETS_EXTENSION_URI));
    realtime_state->parameter_count.store(parameters && parameters->get_parameter_count ?
            parameters->get_parameter_count(parameters, plugin) : -1, std::memory_order_release);
    realtime_state->preset_count.store(presets && presets->get_preset_count ?
            presets->get_preset_count(presets, plugin) : 0, std::memory_order_release);
    realtime_state->poll_replies_ready.store(true, std::memory_order_release);
}

bool aap::LocalPluginInstance::cachedPollReply(AAPXSDefinition* definition, AAPXSRequestContext& request) {
    if (!realtime_state->poll_replies_ready.load(std::memory_order_acquire) || !definition || !definition->uri ||
        !request.serialization || !request.serialization->data || request.serialization->data_capacity < sizeof(int32_t))
        return false;
    // Only the standard handlers have snapshot semantics. A custom handler
    // using the same URI or an RT-safe flag does not grant concurrent access.
    int32_t value;
    if (!strcmp(definition->uri, AAP_PARAMETERS_EXTENSION_URI) && request.opcode == OPCODE_PARAMETERS_GET_PARAMETER_COUNT) {
        if (definition->process_incoming_plugin_aapxs_request != xs::AAPXSDefinition_Parameters{}.asPublic().process_incoming_plugin_aapxs_request)
            return false;
        value = realtime_state->parameter_count.load(std::memory_order_acquire);
    } else if (!strcmp(definition->uri, AAP_PRESETS_EXTENSION_URI) && request.opcode == OPCODE_GET_PRESET_COUNT) {
        if (definition->process_incoming_plugin_aapxs_request != xs::AAPXSDefinition_Presets{}.asPublic().process_incoming_plugin_aapxs_request)
            return false;
        value = realtime_state->preset_count.load(std::memory_order_acquire);
    } else return false;
    memcpy(request.serialization->data, &value, sizeof(value));
    request.serialization->data_size = sizeof(value);
    return true;
}

void aap::LocalPluginInstance::pollExtensionWorker() {
    if (realtime_state->process_notification.exchange(false, std::memory_order_acq_rel))
        ((PluginService*) host)->requestProcessToHost(instance_id);
    auto notifications = realtime_state->standard_notifications.exchange(0, std::memory_order_acq_rel);
    if (notifications && !realtime_state->poll_replies_ready.load(std::memory_order_acquire)) {
        // A factory can notify before its plugin pointer/extensions are ready.
        // Preserve the bits; setup/prepare will wake the worker after publishing.
        realtime_state->standard_notifications.fetch_or(notifications, std::memory_order_release);
        notifications = 0;
    }
    if (notifications) {
        // Publish fresh counts before forwarding a change notification or
        // replying to queued polls. Refresh once per batch, not once per poll.
        const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
        refreshPollReplies();
    }
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
    if (notifications & 1) send(AAP_PARAMETERS_EXTENSION_URI, OPCODE_NOTIFY_PARAMETERS_CHANGED, aapxsRequestIdSerial());
    if (notifications & 2) send(AAP_PRESETS_EXTENSION_URI, OPCODE_NOTIFY_PRESET_LOADED, aapxsRequestIdSerial());
    if (notifications & 4) send(AAP_PRESETS_EXTENSION_URI, OPCODE_NOTIFY_PRESETS_UPDATED, aapxsRequestIdSerial());
    for (unsigned n = 0; n < 64 && !realtime_state->worker.isStopping(); ++n) {
        if (!realtime_state->host_notifications.tryConsume([&](void* data, size_t) {
            auto& notification = *static_cast<internal::HostNotification*>(data);
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
    refreshPollReplies();
    internal::setParameterLayoutRefreshReady(*this);
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
        auto found = store->getExtensionUriToIndexMap().find(uri);
        if (found == store->getExtensionUriToIndexMap().end()) return;
        auto index = found->second;
        serialization->data = store->getExtensionBuffer(index);
        serialization->data_capacity = store->getExtensionBufferCapacity(index);
    };
    aapxs_dispatcher.setupInstances(this,
                                    func,
                                    staticSendAAPXSReply,
                                    staticSendAAPXSRequest,
                                    staticGetNewRequestId);
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
    startExtensionWorker();
}

void
aap::LocalPluginInstance::sendPluginAAPXSReply(AAPXSRequestContext* request) {
    if (auto reply = xs::retainAAPXSReply(request)) reply->complete();
    // Borrowed Binder requests complete synchronously when extension() returns.
}

bool
aap::LocalPluginInstance::sendHostAAPXSRequest(AAPXSRequestContext* request) {
    if (realtime_state->worker.isStopping()) return false;
    bool hasPayload = request->serialization && request->serialization->data_size > 0;
    if (!request->callback && !request->error_callback && !hasPayload && request->uri) {
        uint32_t bit = 0;
        if (!strcmp(request->uri, AAP_PARAMETERS_EXTENSION_URI) && request->opcode == OPCODE_NOTIFY_PARAMETERS_CHANGED) bit = 1;
        if (!strcmp(request->uri, AAP_PRESETS_EXTENSION_URI)) {
            if (request->opcode == OPCODE_NOTIFY_PRESET_LOADED) bit = 2;
            if (request->opcode == OPCODE_NOTIFY_PRESETS_UPDATED) bit = 4;
        }
        if (bit) {
            realtime_state->standard_notifications.fetch_or(bit, std::memory_order_release);
            realtime_state->worker.notify();
        }
        else {
            internal::HostNotification notification{};
            auto length = strnlen(request->uri, sizeof(notification.uri));
            if (length == sizeof(notification.uri)) return false;
            memcpy(notification.uri, request->uri, length + 1);
            notification.opcode = request->opcode;
            notification.request_id = request->request_id;
            if (realtime_state->host_notifications.tryPush(&notification, sizeof(notification)))
                realtime_state->worker.notify();
        }
        return false; // notifications have no reply
    }
    // General host queries require a non-processing caller. Rejection retains the
    // caller's context and does not invoke an arbitrary callback on this thread.
    if (RealtimeScope::isActive()) return false;
    if (request->uri && !realtime_state->worker.isCurrentThread() &&
        ((!strcmp(request->uri, AAP_PARAMETERS_EXTENSION_URI) && request->opcode == OPCODE_NOTIFY_PARAMETERS_CHANGED) ||
         (!strcmp(request->uri, AAP_PRESETS_EXTENSION_URI) &&
          (request->opcode == OPCODE_NOTIFY_PRESET_LOADED || request->opcode == OPCODE_NOTIFY_PRESETS_UPDATED)))) {
        const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
        refreshPollReplies();
    }
    if (request->opcode == OPCODE_NOTIFY_PARAMETERS_CHANGED && request->uri && !strcmp(request->uri, AAP_PARAMETERS_EXTENSION_URI))
        internal::requestParameterLayoutRefresh(*this);

    auto& dispatcher = getAAPXSDispatcher();
    auto aapxsInstance = request->urid != 0 ? dispatcher.getHostAAPXSByUrid(request->urid) : dispatcher.getHostAAPXSByUri(request->uri);
    if (!aapxsInstance || !aapxsInstance->serialization)
        return false;
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
    if (def) {
        auto& dispatcher = getAAPXSDispatcher();
        auto instance = urid != 0 ? dispatcher.getPluginAAPXSByUrid(urid) : dispatcher.getPluginAAPXSByUri(uri.c_str());
        if (instance && instance->serialization) {
            AAPXSRequestContext request{nullptr, nullptr, instance->serialization, urid, uri.c_str(), requestId, opcode};
            if (cachedPollReply(def, request)) {
                dispatcher.publishBinderReplySize(instance->serialization);
                return;
            }
        }
    }
    const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
    // special case URID mapping request: this hosting implementation also consumes it and
    // adds the URID mapping.
    // Note that it is handled only at UNPREPARED state and thus no realtime special casing happens.
    if (urid == 0 && uri == AAP_URID_EXTENSION_URI) {
        auto instance = getAAPXSDispatcher().getPluginAAPXSByUri(uri.c_str());
        auto parsedUrid = *(uint8_t*) instance->serialization->data;
        auto len = *(int32_t*) (uint8_t*) instance->serialization->data + 1;
        auto s = (char*) calloc(len + 1, 1);
        strncpy(s, (char*) instance->serialization->data + 1 + sizeof(int32_t), len);
        s[len] = 0;
        urid_mapping.forceAdd(parsedUrid, s);
        free(s);
    } // ... and the mapping could also be used by the plugin, so go on as well.


    if (def) { // ignore undefined extensions here
        auto& dispatcher = getAAPXSDispatcher();
        auto instance = urid != 0 ? dispatcher.getPluginAAPXSByUrid(urid) : dispatcher.getPluginAAPXSByUri(uri.c_str());
        dispatcher.receiveBinderRequest(instance->serialization);
        AAPXSRequestContext context{nullptr, nullptr, instance->serialization, urid, uri.c_str(), requestId, opcode};
        // RT-safe does not imply safe concurrent access to plugin state. Handlers
        // accessing plugin state still run between DSP blocks; count snapshots
        // were handled above without entering this control section.
        def->process_incoming_plugin_aapxs_request(def, instance, plugin, &context);
        refreshPollReplies();
        dispatcher.publishBinderReplySize(instance->serialization);
    }
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
        if (cachedPollReply(def, reply->request())) {
            reply->complete();
            return;
        }
        const internal::ProcessingQuiescence::Control suspension{realtime_state->processing};
        def->process_incoming_plugin_aapxs_request(def, instance, plugin, &reply->request());
        refreshPollReplies();
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
