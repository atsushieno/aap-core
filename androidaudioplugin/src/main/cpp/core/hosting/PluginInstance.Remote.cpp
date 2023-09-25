#include "aap/core/host/plugin-instance.h"
#include "aap/core/host/shared-memory-store.h"
#include "../AAPJniFacade.h"

#define LOG_TAG "AAP.Remote.Instance"

aap::RemotePluginInstance::RemotePluginInstance(PluginClient* client,
                                                AAPXSRegistry* aapxsRegistry,
                                                const PluginInformation* pluginInformation,
                                                AndroidAudioPluginFactory* loadedPluginFactory,
                                                int32_t sampleRate,
                                                int32_t eventMidi2InputBufferSize)
        : PluginInstance(pluginInformation, loadedPluginFactory, sampleRate, eventMidi2InputBufferSize),
          client(client),
          aapxs_registry(aapxsRegistry),
          standards(this),
          aapxs_manager(std::make_unique<RemoteAAPXSManager>(this)) {
    shared_memory_store = new ClientPluginSharedMemoryStore();
}

void aap::RemotePluginInstance::configurePorts() {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to configurePorts() at state: %d (instanceId: %d)",
                     instantiation_state, instance_id);
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
    plugin_host_facade.get_extension = RemotePluginInstance::staticGetExtension;
    return &plugin_host_facade;
}

void aap::RemotePluginInstance::sendExtensionMessage(const char *uri, int32_t dataSize, int32_t opcode) {
    auto aapxsInstance = aapxs_manager->getInstanceFor(uri);
    // Here we have to get a native plugin instance and send extension message.
    // It is kind af annoying because we used to implement Binder-specific part only within the
    // plugin API (binder-client-as-plugin.cpp)...
    // So far, instead of rewriting a lot of code to do so, we let AAPClientContext
    // assign its implementation details that handle Binder messaging as a std::function.
    send_extension_message_impl(aapxsInstance->uri, getInstanceId(), dataSize, opcode);
}

void aap::RemotePluginInstance::prepare(int frameCount) {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG,
                     "Unexpected call to prepare() at state: %d (instanceId: %d)",
                     instantiation_state, instance_id);
        return;
    }

    auto numPorts = getNumPorts();
    auto shm = dynamic_cast<aap::ClientPluginSharedMemoryStore*>(getSharedMemoryStore());
    auto code = shm->allocateClientBuffer(numPorts, frameCount, *this, DEFAULT_CONTROL_BUFFER_SIZE);
    if (code != aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS) {
        aap::a_log(AAP_LOG_LEVEL_ERROR, LOG_TAG, aap::PluginSharedMemoryStore::getMemoryAllocationErrorMessage(code));
    }

    plugin->prepare(plugin, getAudioPluginBuffer());
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
    assert(native_ui_controller);
	return AAPJniFacade::getInstance()->getRemoteNativeView(client, this);
#else
    // cannot do anything
    return nullptr;
#endif
}

void aap::RemotePluginInstance::connectRemoteNativeView(int32_t width, int32_t height)  {
#if ANDROID
    assert(native_ui_controller);
	AAPJniFacade::getInstance()->connectRemoteNativeView(client, this, width, height);
#else
    // cannot do anything
#endif
}

uint32_t aapxs_request_id_serial{0};
uint32_t aap::RemotePluginInstance::aapxsSysEx8RequestSerial() {
    return aapxs_request_id_serial++;
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


//----

AAPXSClientInstance* aap::RemoteAAPXSManager::setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) {
    const char* uri = aapxsClientInstances.addOrGetUri(feature->uri);
    assert (aapxsClientInstances.get(uri) == nullptr);
    if (dataSize < 0)
        dataSize = feature->shared_memory_size;
    aapxsClientInstances.add(uri, std::make_unique<AAPXSClientInstance>(AAPXSClientInstance{owner, uri, owner->instance_id, nullptr, dataSize}));
    auto ret = aapxsClientInstances.get(uri);
    ret->extension_message = staticSendExtensionMessage;
    return ret;
}

void aap::RemoteAAPXSManager::staticSendExtensionMessage(AAPXSClientInstance* clientInstance, int32_t dataSize, int32_t opcode) {
    auto thisObj = (RemotePluginInstance*) clientInstance->host_context;
    thisObj->sendExtensionMessage(clientInstance->uri, dataSize, opcode);
}

