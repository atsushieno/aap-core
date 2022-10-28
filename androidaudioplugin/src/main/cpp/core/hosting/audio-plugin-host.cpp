
// As a principle, there must not be Android-specific references in this code.
// Anything Android specific must to into `android` directory.
#include <sys/stat.h>
#include <sys/mman.h>
#include <vector>
#include "aap/core/host/audio-plugin-host.h"
#include "aap/unstable/logging.h"
#include "shared-memory-store.h"
#include "audio-plugin-host-internals.h"
#include "aap/core/host/plugin-client-system.h"
#include "../extensions/presets-service.h"
#include "../extensions/midi2-service.h"
#include "../extensions/port-config-service.h"


namespace aap
{


//-----------------------------------

PluginInformation::PluginInformation(bool isOutProcess, const char* pluginPackageName,
									 const char* pluginLocalName, const char* displayName,
									 const char* manufacturerName, const char* versionString,
									 const char* pluginID, const char* sharedLibraryFilename,
									 const char* libraryEntrypoint, const char* metadataFullPath,
									 const char* primaryCategory)
			: is_out_process(isOutProcess),
			  plugin_package_name(pluginPackageName),
			  plugin_local_name(pluginLocalName),
			  display_name(displayName),
			  manufacturer_name(manufacturerName),
			  version(versionString),
			  shared_library_filename(sharedLibraryFilename),
			  library_entrypoint(libraryEntrypoint),
			  plugin_id(pluginID),
			  metadata_full_path(metadataFullPath),
			  primary_category(primaryCategory)
{
	struct tm epoch{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	last_info_updated_unixtime_milliseconds = (int64_t) (1000.0 * difftime(time(nullptr), mktime(&epoch)));

	char *cp;
	size_t len = (size_t) snprintf(nullptr, 0, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
	cp = (char*) calloc(len, 1);
	snprintf(cp, len, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
	identifier_string = cp;
	free(cp);
}


//-----------------------------------

PluginBuffer::~PluginBuffer() {
	if (buffer) {
		for (size_t i = 0; i < buffer->num_buffers; i++)
			free(buffer->buffers[i]);
		free(buffer->buffers);
	}
}

bool PluginBuffer::allocateBuffer(size_t numPorts, size_t numFrames, PluginInstance& instance, size_t defaultControlBytesPerBlock) {
	assert(!buffer); // already allocated

	buffer = std::make_unique<AndroidAudioPluginBuffer>();
	if (!buffer)
		return false;

	size_t defaultAudioMemSize = numFrames * sizeof(float);
	buffer->num_buffers = numPorts;
	buffer->num_frames = numFrames;
	buffer->buffers = (void **) calloc(numPorts, sizeof(float *));
	if (!buffer->buffers)
		return false;

	for (size_t i = 0; i < numPorts; i++) {
		size_t defaultMemSize = instance.getPort(i)->getContentType() != AAP_CONTENT_TYPE_AUDIO ? defaultControlBytesPerBlock : defaultAudioMemSize;
		int minSize = instance.getPort(i)->getPropertyAsInteger(AAP_PORT_MINIMUM_SIZE);
        int memSize = std::max(minSize, (int) defaultMemSize);
		buffer->buffers[i] = calloc(1, memSize);
		if (!buffer->buffers[i])
			return false;
	}

	return true;
}

//-----------------------------------

int32_t PluginSharedMemoryStore::allocateClientBuffer(size_t numPorts, size_t numFrames) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_LOCAL;

	size_t memSize = numFrames * sizeof(float);
	port_buffer->num_buffers = numPorts;
	port_buffer->num_frames = numFrames;
	port_buffer->buffers = (void **) calloc(numPorts, sizeof(float *));
	if (!port_buffer->buffers)
		return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC;

	for (size_t i = 0; i < numPorts; i++) {
		int32_t fd = PluginClientSystem::getInstance()->createSharedMemory(memSize);
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		port_buffer_fds->emplace_back(fd);
        port_buffer->buffers[i] = mmap(nullptr, memSize,
								  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!port_buffer->buffers[i])
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

int32_t PluginSharedMemoryStore::allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_REMOTE;

	size_t numPorts = clientFDs.size();
	size_t memSize = numFrames * sizeof(float);
	port_buffer->num_buffers = numPorts;
	port_buffer->num_frames = numFrames;
	port_buffer->buffers = (void **) calloc(numPorts, sizeof(float *));
	if (!port_buffer->buffers)
		return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC;

	for (size_t i = 0; i < numPorts; i++) {
		int32_t fd = clientFDs[i];
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		port_buffer_fds->emplace_back(fd);
        port_buffer->buffers[i] = mmap(nullptr, memSize,
								  PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!port_buffer->buffers[i])
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

//-----------------------------------

std::unique_ptr<StateExtensionFeature> aapxs_state{nullptr};
std::unique_ptr<PresetsExtensionFeature> aapxs_presets{nullptr};
std::unique_ptr<PortConfigExtensionFeature> aapxs_port_config{nullptr};
std::unique_ptr<Midi2ExtensionFeature> aapxs_midi2{nullptr};

PluginHost::PluginHost(PluginListSnapshot* contextPluginList)
	: plugin_list(contextPluginList)
{
	assert(contextPluginList);

	aapxs_registry = std::make_unique<AAPXSRegistry>();

	// state
	if (aapxs_state == nullptr)
		aapxs_state = std::make_unique<StateExtensionFeature>();
	aapxs_registry->add(aapxs_state->asPublicApi());
	// presets
	if (aapxs_presets == nullptr)
		aapxs_presets = std::make_unique<PresetsExtensionFeature>();
	aapxs_registry->add(aapxs_presets->asPublicApi());
	// port_config
	if (aapxs_port_config == nullptr)
		aapxs_port_config = std::make_unique<PortConfigExtensionFeature>();
	aapxs_registry->add(aapxs_state->asPublicApi());
	// midi2
	if (aapxs_midi2 == nullptr)
		aapxs_midi2 = std::make_unique<Midi2ExtensionFeature>();
	aapxs_registry->add(aapxs_midi2->asPublicApi());
}

AAPXSFeature* PluginHost::getExtensionFeature(const char* uri) {
	return aapxs_registry->getByUri(uri);
}

void PluginHost::destroyInstance(PluginInstance* instance)
{
	instances.erase(std::find(instances.begin(), instances.end(), instance));
	delete instance;
}

PluginInstance* PluginHost::instantiateLocalPlugin(const PluginInformation *descriptor, int sampleRate)
{
	dlerror(); // clean up any previous error state
	auto file = descriptor->getLocalPluginSharedLibrary();
	auto metadataFullPath = descriptor->getMetadataFullPath();
	if (!metadataFullPath.empty()) {
		size_t idx = metadataFullPath.find_last_of('/');
		if (idx > 0) {
			auto soFullPath = metadataFullPath.substr(0, idx + 1) + file;
			struct stat st;
			if (stat(soFullPath.c_str(), &st) == 0)
				file = soFullPath;
		}
	}
	auto entrypoint = descriptor->getLocalPluginLibraryEntryPoint();
	auto dl = dlopen(file.length() > 0 ? file.c_str() : "libandroidaudioplugin.so", RTLD_LAZY);
	if (dl == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP library %s could not be loaded.\n", file.c_str());
		return nullptr;
	}
	auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint.length() > 0 ? entrypoint.c_str() : "GetAndroidAudioPluginFactory");
	if (factoryGetter == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP factory %s was not found in %s.\n", entrypoint.c_str(), file.c_str());
		return nullptr;
	}
	auto pluginFactory = factoryGetter();
	if (pluginFactory == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP factory %s could not instantiate a plugin.\n", entrypoint.c_str());
		return nullptr;
	}
	auto instance = new LocalPluginInstance(this, static_cast<int32_t>(instances.size()), descriptor, pluginFactory, sampleRate);
	instances.emplace_back(instance);
	return instance;
}

PluginClient::Result<int32_t> PluginClient::createInstance(std::string identifier, int sampleRate, bool isRemoteExplicit)
{
	Result<int32_t> result;
	createInstanceImpl(false, identifier, sampleRate, isRemoteExplicit, [&result](int32_t instanceId, std::string error) {
		result.value = instanceId;
		result.error = error;
	});
	return result;
}

void PluginClient::createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> userCallback)
{
	createInstanceImpl(true, identifier, sampleRate, isRemoteExplicit, userCallback);
}

void PluginClient::createInstanceImpl(bool canDynamicallyConnect, std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string)> userCallback)
{
	const PluginInformation *descriptor = plugin_list->getPluginInformation(identifier);
	assert (descriptor != nullptr);

	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	auto internalCallback = [=](PluginInstance* instance, std::string error) {
		if (instance != nullptr) {
			userCallback(instance->getInstanceId(), "");
		}
		else
			userCallback(-1, error);
	};
	if (isRemoteExplicit || descriptor->isOutProcess())
		instantiateRemotePlugin(canDynamicallyConnect, descriptor, sampleRate, internalCallback);
	else {
		try {
			auto instance = instantiateLocalPlugin(descriptor, sampleRate);
			internalCallback(instance, "");
		} catch(std::exception& ex) {
			internalCallback(nullptr, ex.what());
		}
	}
}

void PluginClient::instantiateRemotePlugin(bool canDynamicallyConnect, const PluginInformation *descriptor, int sampleRate, std::function<void(PluginInstance*, std::string)> callback)
{
    // We first ensure to bind the remote plugin service, and then create a plugin instance.
    //  Since binding the plugin service must be asynchronous while instancing does not have to be,
    //  we call ensureServiceConnected() first and pass the rest as its callback.
    auto internalCallback = [=](std::string error) {
        if (error.empty()) {
#if ANDROID
            auto pluginFactory = GetAndroidAudioPluginFactoryClientBridge(this);
#else
            auto pluginFactory = GetDesktopAudioPluginFactoryClientBridge();
#endif
            assert (pluginFactory != nullptr);
            auto instance = new RemotePluginInstance(this, static_cast<int32_t>(instances.size()), descriptor, pluginFactory, sampleRate);
            instances.emplace_back(instance);
            callback(instance, "");
        }
        else
            callback(nullptr, error);
    };
    auto service = connections->getServiceHandleForConnectedPlugin(descriptor->getPluginPackageName(), descriptor->getPluginLocalName());
    if (service != nullptr)
        internalCallback("");
    else if (!canDynamicallyConnect)
        internalCallback(std::string{"Plugin service is not started yet: "} + descriptor->getPluginID());
    else
        PluginClientSystem::getInstance()->ensurePluginServiceConnected(connections, descriptor->getPluginPackageName(), internalCallback);
}

int32_t PluginService::createInstance(std::string identifier, int sampleRate)  {
	auto info = plugin_list->getPluginInformation(identifier);
	if (!info) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, "libandroidaudioplugin", "Plugin information was not found for: %s ", identifier.c_str());
		return -1;
	}
	instantiateLocalPlugin(info, sampleRate);
	return instances.size() - 1;
}

//-----------------------------------

PluginInstance::PluginInstance(int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate)
		: sample_rate(sampleRate),
		  instance_id(instanceId),
		  plugin_factory(loadedPluginFactory),
		  instantiation_state(PLUGIN_INSTANTIATION_STATE_INITIAL),
		  plugin(nullptr),
		  pluginInfo(pluginInformation) {
	assert(pluginInformation);
	assert(loadedPluginFactory);

	aapxs_shared_memory_store = new PluginSharedMemoryStore();
}

PluginInstance::~PluginInstance() { dispose(); }

void PluginInstance::dispose() {
	instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
	if (plugin != nullptr)
		plugin_factory->release(plugin_factory, plugin);
	plugin = nullptr;
	delete aapxs_shared_memory_store;
}

int32_t PluginInstance::allocateAudioPluginBuffer(size_t numPorts, size_t numFrames, size_t defaultControlBytesPerBlock) {
	assert(!plugin_buffer);
	plugin_buffer = std::make_unique<PluginBuffer>();
	return plugin_buffer->allocateBuffer(numPorts, numFrames, *this, defaultControlBytesPerBlock);
}

AndroidAudioPluginBuffer* PluginInstance::getAudioPluginBuffer(size_t numPorts, size_t numFrames, size_t defaultControlBytesPerBlock) {
	if (!plugin_buffer) {
        assert(numPorts >= 0);
        assert(numFrames > 0);
        assert(defaultControlBytesPerBlock > 0);
        allocateAudioPluginBuffer(numPorts, numFrames, defaultControlBytesPerBlock);
    }
	return plugin_buffer->getAudioPluginBuffer();
}

void PluginInstance::completeInstantiation()
{
	assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_INITIAL);

	AndroidAudioPluginHost* asPluginAPI = getHostFacadeForCompleteInstantiation();
	plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), sample_rate, asPluginAPI);
	assert(plugin);

	instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
}

//----

RemotePluginInstance::RemotePluginInstance(PluginClient *client, int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate)
        : PluginInstance(instanceId, pluginInformation, loadedPluginFactory, sampleRate),
          client(client),
          standards(this),
          aapxs_manager(std::make_unique<RemoteAAPXSManager>(this)) {
}

void RemotePluginInstance::configurePorts() {
    assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED);

	startPortConfiguration();

	auto ext = plugin->get_extension(plugin, AAP_PORT_CONFIG_EXTENSION_URI);
	if (ext != nullptr) {
			// configure ports using port-config extension.

			// FIXME: implement

	} else if (pluginInfo->getNumDeclaredPorts() == 0)
		setupPortConfigDefaults();
	else
		setupPortsViaMetadata();
}

void PluginInstance::setupPortConfigDefaults() {
	// If there is no declared ports, apply default ports configuration.
	uint32_t nPort = 0;

	if (pluginInfo->isInstrument()) {
		PortInformation midi_in{nPort++, "MIDI In", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
		PortInformation midi_out{nPort++, "MIDI Out", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_OUTPUT};
		configured_ports->emplace_back(midi_in);
		configured_ports->emplace_back(midi_out);
	} else {
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

void PluginInstance::scanParametersAndBuildList() {
	assert(!cached_parameters);

	// FIXME: it's better to populate ParameterInformation from PortInformation for softer migration.

	auto ext = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
	if (!ext)
		return;

	// if parameters extension does not exist, do not populate cached parameters list.
	// (The empty list means no parameters in metadata either.)
	cached_parameters = std::make_unique<std::vector<ParameterInformation>>();

	// FIXME: pass appropriate context
	auto target = AndroidAudioPluginExtensionTarget{plugin, nullptr};

	parameter_mapping_policy = ext->get_mapping_policy(target);

	for (auto i = 0, n = ext->get_parameter_count(target); i < n; i++) {
		auto para = ext->get_parameter(target, i);
		ParameterInformation p{para->stable_id, para->display_name, para->min_value, para->max_value, para->default_value};
		cached_parameters->emplace_back(p);
	}
}

AndroidAudioPluginHost* RemotePluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension_data = nullptr; // we shouldn't need it.
    return &plugin_host_facade;
}

void RemotePluginInstance::sendExtensionMessage(const char *uri, int32_t opcode) {
    auto aapxsInstance = aapxs_manager->getInstanceFor(uri);
    // Here we have to get a native plugin instance and send extension message.
    // It is kind af annoying because we used to implement Binder-specific part only within the
    // plugin API (binder-client-as-plugin.cpp)...
    // So far, instead of rewriting a lot of code to do so, we let AAPClientContext
    // assign its implementation details that handle Binder messaging as a std::function.
    send_extension_message_impl(aapxsInstance->uri, getInstanceId(), opcode);
}

//----

LocalPluginInstance::LocalPluginInstance(PluginHost *service, int32_t instanceId, const PluginInformation* pluginInformation, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate)
        : PluginInstance(instanceId, pluginInformation, loadedPluginFactory, sampleRate),
          service(service),
          aapxsServiceInstances([&]() { return getPlugin(); }),
          standards(this) {
}

AndroidAudioPluginHost* LocalPluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension_data = internalGetExtensionData;
    return &plugin_host_facade;
}

//----

AndroidAudioPlugin* RemotePluginInstanceStandardExtensionsImpl::getPlugin() { return owner->getPlugin(); }

AAPXSClientInstanceManager* RemotePluginInstanceStandardExtensionsImpl::getAAPXSManager() {
	return owner->getAAPXSManager();
}

AndroidAudioPlugin* LocalPluginInstanceStandardExtensionsImpl::getPlugin() { return owner->getPlugin(); }

//----

AAPXSClientInstance* RemoteAAPXSManager::setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) {
	const char* uri = aapxsClientInstances.addOrGetUri(feature->uri);
	assert (aapxsClientInstances.get(uri) == nullptr);
	if (dataSize < 0)
		dataSize = feature->shared_memory_size;
	aapxsClientInstances.add(uri, std::make_unique<AAPXSClientInstance>(AAPXSClientInstance{owner, uri, owner->getInstanceId(), nullptr, dataSize}));
	auto ret = aapxsClientInstances.get(uri);
	ret->extension_message = staticSendExtensionMessage;
	return ret;
}

void RemoteAAPXSManager::staticSendExtensionMessage(AAPXSClientInstance* clientInstance, int32_t opcode) {
	auto thisObj = (RemotePluginInstance*) clientInstance->host_context;
	thisObj->sendExtensionMessage(clientInstance->uri, opcode);
}
} // namespace
