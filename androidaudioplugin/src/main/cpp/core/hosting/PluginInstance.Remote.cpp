#include <algorithm>
#include "aap/core/host/plugin-instance.h"
#include "plugin-parameter-state.h"
#include "aapxs-transport.h"
#include "aapxs-midi2-session-internal.h"
#include "midi2-port-buffer.h"
#include "instance-realtime-state.h"
#include "aap/core/host/shared-memory-store.h"
#include "../AAPJniFacade.h"
#include "aap/core/aap_midi2_helper.h"
#include "../include_cmidi2.h"

#define LOG_TAG "AAP.Remote.Instance"

aap::RemotePluginInstance::RemotePluginInstance(PluginClient* client,
                                                xs::AAPXSDefinitionRegistry *aapxsRegistry,
                                                const PluginInformation* pluginInformation,
                                                AndroidAudioPluginFactory* loadedPluginFactory,
                                                int32_t eventMidi2InputBufferSize)
        : PluginInstance(pluginInformation, loadedPluginFactory, eventMidi2InputBufferSize),
          client(client),
          aapxs_session(eventMidi2InputBufferSize),
          feature_registry(new xs::AAPXSDefinitionClientRegistry(aapxsRegistry)),
          aapxs_dispatcher(aapxsRegistry),
          standards(std::make_unique<xs::ClientStandardExtensions>())
          {
    shared_memory_store = new ClientPluginSharedMemoryStore();

    aapxs_session.setReplyHandler([&](aap_midi2_aapxs_parse_context* context) {
        handleAAPXSReply(context);
    });
    internal::AAPXSMidi2SessionAccess::setDeadlineChangedHandler(aapxs_session, [this] {
        realtime_state->worker.notify();
    });
}

std::chrono::steady_clock::time_point aap::RemotePluginInstance::nextExtensionDeadline() {
    return internal::AAPXSMidi2SessionAccess::nextDeadline(aapxs_session);
}

aap::RemotePluginInstance::~RemotePluginInstance() {
    stopExtensionWorker();
    abortAllPendingAAPXS("AAPXS instance destroyed");
    releasePlugin();
}

void aap::RemotePluginInstance::pollExtensionWorker() {
    // A nested blocking call from a completion must not wait for this worker to
    // consume another SysEx8 reply. Binder delivery runs independently.
    internal::ScopedBinderOnlyAAPXS binderOnly;
    for (unsigned n = 0; n < 64 && !realtime_state->worker.isStopping(); ++n) {
        if (!realtime_state->aapxs_input.tryConsume([this](void* data, size_t) {
            aapxs_session.completeSession(data, plugin);
            return true;
        })) break;
    }
    // Timeouts must also progress while processing has stopped or no MIDI arrives.
    AAPMidiBufferHeader empty{};
    if (!realtime_state->worker.isStopping()) aapxs_session.completeSession(&empty, plugin);
}

void aap::RemotePluginInstance::configurePorts() {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to configurePorts() at state: %d (instanceId: %d)",
                     instantiation_state.load(), instance_id);
        return;
    }

    startPortConfiguration();

    auto ext = plugin->get_extension(plugin, AAP_PORT_CONFIG_EXTENSION_URI);
    if (ext != nullptr) {
        // configure ports using port-config extension.

        // FIXME: implement
        assert(false);

    } else if (pluginInfo->getNumDeclaredPorts() == 0)
        setupPortConfigDefaults();
    else
        setupPortsViaMetadata();
}


AndroidAudioPluginHost* aap::RemotePluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = RemotePluginInstance::staticGetHostExtension;
    return &plugin_host_facade;
}

void aap::RemotePluginInstance::prepare(int frameCount, int32_t sampleRate) {
    prepare(frameCount, sampleRate, DEFAULT_CONTROL_BUFFER_SIZE);
}

void aap::RemotePluginInstance::prepare(int frameCount, int32_t sampleRate, int32_t controlBytesPerBlock) {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to prepare() at state: %d (instanceId: %d)",
                     instantiation_state.load(), instance_id);
        return;
    }

    sample_rate = sampleRate;
    auto numPorts = getNumPorts();
    auto shm = dynamic_cast<aap::ClientPluginSharedMemoryStore*>(getSharedMemoryStore());
    auto code = shm->allocateClientBuffer(numPorts, frameCount, *this, controlBytesPerBlock > 0 ? controlBytesPerBlock : DEFAULT_CONTROL_BUFFER_SIZE);
    if (code != aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS) {
        aap::a_log(AAP_LOG_LEVEL_ERROR, LOG_TAG, aap::PluginSharedMemoryStore::getMemoryAllocationErrorMessage(code));
    }

    plugin->prepare(plugin, sample_rate, getAudioPluginBuffer());
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
}

void* aap::RemotePluginInstance::getRemoteWebView()  {
#if ANDROID
    return aap::AAPJniFacade::getInstance()->getRemoteWebView(client, this);
#else
    // cannot do anything
    return nullptr;
#endif
}

void aap::RemotePluginInstance::prepareSurfaceControlForRemoteNativeUI() {
#if ANDROID
    if (!native_ui_controller)
		native_ui_controller = std::make_unique<RemotePluginNativeUIController>(this);
#else
    // cannot do anything
#endif
}

void* aap::RemotePluginInstance::getRemoteNativeView()  {
#if ANDROID
    if (!native_ui_controller) {
        AAP_ASSERT_FALSE; // should not happen
        return nullptr;
    }
	return AAPJniFacade::getInstance()->getRemoteNativeView(client, this);
#else
    // cannot do anything
    return nullptr;
#endif
}

bool aap::RemotePluginInstance::getRemoteNativeViewPreferredSize(int32_t& width, int32_t& height)  {
#if ANDROID
    if (!native_ui_controller) {
        AAP_ASSERT_FALSE; // should not happen
        return false;
    }
    return AAPJniFacade::getInstance()->getRemoteNativeViewPreferredSize(client, this, width, height);
#else
    (void) width;
    (void) height;
    return false;
#endif
}

void aap::RemotePluginInstance::connectRemoteNativeView(int32_t width, int32_t height)  {
#if ANDROID
    if (!native_ui_controller) {
        AAP_ASSERT_FALSE; // should not happen
        return;
    }
	AAPJniFacade::getInstance()->connectRemoteNativeView(client, this, width, height);
#else
    // cannot do anything
#endif
}

void aap::RemotePluginInstance::configureRemoteNativeView(
    int32_t viewportWidth,
    int32_t viewportHeight,
    int32_t contentWidth,
    int32_t contentHeight,
    int32_t scrollX,
    int32_t scrollY)
{
#if ANDROID
    if (!native_ui_controller) {
        AAP_ASSERT_FALSE; // should not happen
        return;
    }
    AAPJniFacade::getInstance()->configureRemoteNativeView(
        client,
        this,
        viewportWidth,
        viewportHeight,
        contentWidth,
        contentHeight,
        scrollX,
        scrollY);
#else
    (void) viewportWidth;
    (void) viewportHeight;
    (void) contentWidth;
    (void) contentHeight;
    (void) scrollX;
    (void) scrollY;
#endif
}

void aap::RemotePluginInstance::process(int32_t frameCount, int32_t timeoutInNanoseconds) {
    RealtimeScope realtime;
    const char* remote_trace_name = "AAP::RemotePluginInstance_process";
    struct timespec timeSpecBegin{}, timeSpecEnd{};
#if ANDROID
    if (ATrace_isEnabled()) {
        ATrace_beginSection(remote_trace_name);
        clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
    }
#endif

    // The plugin resets the input lengths; do not trust them.
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2 && port->getPortDirection() == AAP_PORT_DIRECTION_INPUT)
            internal::getMidi2PortBuffer(getAudioPluginBuffer(), i);
    }

    // merge input from AAPXS SysEx8 into the host's MIDI inputs
    mergeQueuedUmp(AAP_PORT_DIRECTION_INPUT);

    // Keep the parameter value cache in sync with the changes that the host is sending now;
    // otherwise hosts that read values back from the cache (e.g. the compose-app host UI) see
    // their own changes reverted, unless the plugin happens to echo them to its MIDI2 output.
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
            continue;
        auto aapBuffer = getAudioPluginBuffer();
        internal::updateParameterValueCacheFromInputBuffer(*this, aapBuffer->get_buffer(aapBuffer, i));
    }

    // now we can pass the input to the plugin.
    plugin->process(plugin, getAudioPluginBuffer(), frameCount, timeoutInNanoseconds);

    // retrieve AAPXS SysEx8 replies if any.
    for (auto i = 0, n = getNumPorts(); i < n; i++) {
        auto port = getPort(i);
        if (port->getContentType() != AAP_CONTENT_TYPE_MIDI2 ||
            port->getPortDirection() != AAP_PORT_DIRECTION_OUTPUT)
            continue;
        void* data = internal::getMidi2PortBuffer(getAudioPluginBuffer(), i);
        internal::sysex8::filterOutMessages(data, realtime_state.get(), internal::queueAAPXSMidi2Input);
        internal::updateParameterValueCacheFromOutputBuffer(*this, data);
    }

#if ANDROID
    if (ATrace_isEnabled()) {
        clock_gettime(CLOCK_REALTIME, &timeSpecEnd);
        ATrace_setCounter(remote_trace_name,
                          (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 + timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec);
        ATrace_endSection();
    }
#endif
}

namespace {
aap_parameters_host_extension_t hosting_parameters_host_extension{
        nullptr,
        [](aap_parameters_host_extension_t*, AndroidAudioPluginHost* host) {
            aap::internal::requestParameterLayoutRefresh(*(aap::RemotePluginInstance*) host->context);
        }};
}

void *
aap::RemotePluginInstance::internalGetHostExtension(uint8_t urid, const char *uri) {
    if (strcmp(uri, AAP_PLUGIN_INFO_EXTENSION_URI) == 0) {
        return &host_plugin_info;
    }
    if (strcmp(uri, AAP_PARAMETERS_EXTENSION_URI) == 0)
        return &hosting_parameters_host_extension;

    // The host's own implementation takes precedence over the AAPXS-provided receiver.
    if (getHostExtension)
        if (auto hostExtension = getHostExtension(this, urid, uri))
            return hostExtension;

    auto registry = getAAPXSRegistry()->items();
    auto definition = urid != 0 ? registry->getByUrid(urid) : registry->getByUri(uri);
    if (definition && definition->get_host_extension_receiver) {
        auto& dispatcher = getAAPXSDispatcher();
        auto aapxsInstance = urid != 0 ? dispatcher.getHostAAPXSByUrid(urid) : dispatcher.getHostAAPXSByUri(uri);
        if (aapxsInstance) {
            auto receiver = definition->get_host_extension_receiver(definition, aapxsInstance, &plugin_host_facade);
            if (receiver.as_host_extension)
                return receiver.as_host_extension(&receiver);
        }
    }
    return nullptr;
}

//----

aap::RemotePluginInstance::RemotePluginNativeUIController::RemotePluginNativeUIController(RemotePluginInstance* owner) {
    handle = AAPJniFacade::getInstance()->createSurfaceControl();
}

aap::RemotePluginInstance::RemotePluginNativeUIController::~RemotePluginNativeUIController() {
    AAPJniFacade::getInstance()->disposeSurfaceControl(handle);
}

void aap::RemotePluginInstance::RemotePluginNativeUIController::show() {
    AAPJniFacade::getInstance()->showSurfaceControlView(handle);
}

void aap::RemotePluginInstance::RemotePluginNativeUIController::hide() {
    AAPJniFacade::getInstance()->hideSurfaceControlView(handle);
}


// ---- AAPXS v2
static inline bool staticSendAAPXSRequest(AAPXSInitiatorInstance* instance, AAPXSRequestContext* context) {
    return ((aap::RemotePluginInstance *) instance->host_context)->sendPluginAAPXSRequest(context);
}
static inline void staticSendAAPXSReply(AAPXSRecipientInstance* instance, AAPXSRequestContext* context) {
    ((aap::RemotePluginInstance*) instance->host_context)->sendHostAAPXSReply(context);
}

bool aap::RemotePluginInstance::setupAAPXSInstances(std::function<bool(const char*, AAPXSSerializationContext*)> sharedMemoryAllocatingRequester) {
    if (!aapxs_dispatcher.setupInstances(this,
                                           sharedMemoryAllocatingRequester,
                                           staticSendAAPXSRequest,
                                           staticSendAAPXSReply,
                                           staticGetNewRequestId))
        return false;
    standards->initialize(&aapxs_dispatcher);
    for (auto& definition : *getAAPXSRegistry()->items()) {
        if (!definition.uri || !definition.get_plugin_extension_proxy) continue;
        auto urid = getAAPXSRegistry()->items()->getUridMapping()->getUrid(definition.uri);
        auto proxyPointer = standards->asNativePluginExtension(definition.uri);
        if (!proxyPointer) {
            auto initiator = aapxs_dispatcher.getPluginAAPXSByUri(definition.uri);
            auto proxy = definition.get_plugin_extension_proxy(&definition, initiator, initiator->serialization);
            proxyPointer = proxy.as_plugin_extension ? proxy.as_plugin_extension(&proxy) : nullptr;
        }
        plugin_extension_proxies[urid] = proxyPointer;
    }
    startExtensionWorker();
    return true;
}

void aap::RemotePluginInstance::abortAllPendingAAPXS(const std::string& error) {
    internal::AAPXSMidi2SessionAccess::cancelPending(aapxs_session, error.c_str(), plugin);
    internal::abortAAPXSBinderChannels(this, error.c_str(), plugin);
    std::vector<xs::TypedAAPXS*> snapshot;
    {
        std::lock_guard<std::mutex> lock(async_abort_registry->mutex);
        snapshot = async_abort_registry->abortables;
    }
    for (auto* abortable : snapshot)
        abortable->failAllPending(error);
}

bool
aap::RemotePluginInstance::sendPluginAAPXSRequest(uint8_t urid, const char *uri, int32_t opcode, void *data, int32_t dataSize, uint32_t newRequestId) {
    AAPXSSerializationContext serialization{data, (size_t) dataSize, (size_t) dataSize};
    AAPXSRequestContext request{nullptr, nullptr, &serialization, urid, uri, newRequestId, opcode};
    return sendPluginAAPXSRequest(&request);
}

bool
aap::RemotePluginInstance::sendPluginAAPXSRequest(AAPXSRequestContext* request) {
    if (RealtimeScope::isActive() || realtime_state->worker.isStopping()) return false;
    // A request can switch to the RT-safe AAPXS SysEx8 MIDI messaging mode only if the plugin is at
    // ACTIVE state AND the AAPXS itself declares this command (opcode) as RT-safe. Otherwise (including
    // when the extension does not implement is_command_rt_safe at all) it goes to the Binder route.
    auto& dispatcher = getAAPXSDispatcher();
    auto* definition = request->urid != 0
                       ? dispatcher.getDefinitionByUrid(request->urid)
                       : dispatcher.getDefinitionByUri(request->uri);
    bool useSysEx8 =
            !internal::ScopedBinderOnlyAAPXS::isActive() &&
            instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE &&
            definition && definition->is_command_rt_safe &&
            definition->is_command_rt_safe(definition, /*isHostExtension=*/ false, request->opcode);

    if (useSysEx8) {
        // Registration, encoding and cancellation share one session gate. A full table fails
        // through the existing completion contract, without switching to Binder on this thread.
        return internal::AAPXSMidi2SessionAccess::sendRequest(
                aapxs_session, aapxsSessionAddEventUmpInput, this, request);
    }

    auto aapxsInstance = request->urid != 0 ? dispatcher.getPluginAAPXSByUrid(request->urid) : dispatcher.getPluginAAPXSByUri(request->uri);
    if (!aapxsInstance || !aapxsInstance->serialization)
        return false;
    auto channel = internal::getAAPXSBinderChannel(this, aapxsInstance->serialization, [this] {
        return [this](const AAPXSRequestContext& routed) {
            return ipc_send_extension_message_impl(plugin->plugin_specific,
                                                   routed.uri,
                                                   getInstanceId(),
                                                   routed.serialization->data_size,
                                                   routed.request_id,
                                                   routed.opcode,
                                                   routed.callback,
                                                   routed.callback_user_data,
                                                   routed.error_callback);
        };
    });
    return channel->send(request);
}

void
aap::RemotePluginInstance::processPluginAAPXSReply(AAPXSRequestContext* request) {
    if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE) {
        // FIXME: should we handle callback here?
        //if (request->callback)
        //    request->callback(request->callback_user_data, plugin, request->request_id);
    } else {
        // nothing to do when non-realtime mode; extension functions are called synchronously.
    }
}

void
aap::RemotePluginInstance::sendHostAAPXSReply(AAPXSRequestContext* request) {
    (void) request;
    // Host extension requests only arrive via Binder, and complete via its callback: nothing to send.
}

void
aap::RemotePluginInstance::processHostAAPXSRequest(AAPXSRequestContext* context) {
    auto& dispatcher = getAAPXSDispatcher();
    auto registry = feature_registry->items();
    auto aapxs = context->urid != 0 ? registry->getByUrid(context->urid) : registry->getByUri(context->uri);
    if (!aapxs)
        return;

    auto aapxsInstance = context->urid != 0 ? dispatcher.getHostAAPXSByUrid(context->urid) : dispatcher.getHostAAPXSByUri(context->uri);
    if (!aapxsInstance)
        return;

    if (aapxs->process_incoming_host_aapxs_request)
        aapxs->process_incoming_host_aapxs_request(aapxs, aapxsInstance, &plugin_host_facade, context);
}

aap::xs::AAPXSDefinitionClientRegistry *aap::RemotePluginInstance::getAAPXSRegistry() {
    return feature_registry.get();
}

void aap::RemotePluginInstance::setupAAPXS() {
    if (!standards)
        standards = std::make_unique<xs::ClientStandardExtensions>();
}

void aap::RemotePluginInstance::handleAAPXSReply(aap_midi2_aapxs_parse_context *context) {
    auto& dispatcher = getAAPXSDispatcher();
    auto registry = feature_registry->items();
    auto aapxs = context->urid != 0 ? registry->getByUrid(context->urid) : registry->getByUri(context->uri);
    if (aapxs) {
        if (context->opcode >= 0) {
            // plugin AAPXS reply
            auto aapxsInstance = context->urid != 0 ? dispatcher.getPluginAAPXSByUrid(context->urid) : dispatcher.getPluginAAPXSByUri(context->uri);
            // The reply goes into its request's own buffer, never into the shared memory that Binder requests use.
            AAPXSSerializationContext unrequested{context->data, (size_t) context->dataSize, (size_t) context->dataSize};
            auto target = internal::sysex8::findRequestBuffer(context->request_id);
            if (target) {
                auto size = std::min((size_t) context->dataSize, target->data_capacity);
                memcpy(target->data, context->data, size);
                target->data_size = size;
            } else
                target = &unrequested;
            AAPXSRequestContext request{nullptr, nullptr, target, context->urid, context->uri, context->request_id, context->opcode};
            if (aapxs->process_incoming_plugin_aapxs_reply)
                aapxs->process_incoming_plugin_aapxs_reply(aapxs, aapxsInstance, plugin, &request);
            else
                aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "AAPXS %s does not have a reply handler (opcode: %d)", context->uri, context->opcode);
        } else {
            // host AAPXS request
            auto aapxsInstance = context->urid != 0 ? dispatcher.getHostAAPXSByUrid(context->urid) : dispatcher.getHostAAPXSByUri(context->uri);
            memcpy(aapxsInstance->serialization->data, context->data, context->dataSize);
            aapxsInstance->serialization->data_size = context->dataSize;
            AAPXSRequestContext request{nullptr, nullptr, aapxsInstance->serialization, context->urid, context->uri, context->request_id, context->opcode};
            if (aapxs->process_incoming_host_aapxs_request)
                aapxs->process_incoming_host_aapxs_request(aapxs, aapxsInstance, &plugin_host_facade, &request);
            else
                aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "AAPXS %s does not have a reply handler (opcode: %d)", context->uri, context->opcode);
        }
    }
    else
        aap::a_log_f(AAP_LOG_LEVEL_WARN, LOG_TAG, "AAPXS for %s is not registered (opcode: %d)", context->uri, context->opcode);
}

void aap::RemotePluginInstance::setupUrids() {
    auto uridExt = (aap_urid_extension_t*) plugin->get_extension(plugin, AAP_URID_EXTENSION_URI);
    if (!uridExt)
        return; // we cannot use URID, bear with any relevant potential latency!

    auto mapping = getAAPXSRegistry()->items()->getUridMapping();
    for (uint8_t urid : *mapping)
        if (urid != 0)
            uridExt->map(uridExt, plugin, urid, mapping->getUri(urid));
}

void aap::RemotePluginInstance::setupStandardExtensions() {
    setupUrids(); // must be done before initializing AAPXSTypedClients.
    standards->initialize(&aapxs_dispatcher);
}
