
// As a principle, there must not be Android-specific references in this code.
// Anything Android specific must to into `android` directory.
#include <sys/stat.h>
#include <sys/mman.h>
#include <vector>
#include "aap/core/host/audio-plugin-host.h"
#include "aap/unstable/logging.h"
#include "aap/core/host/shared-memory-store.h"
#include "plugin-service-list.h"
#include "audio-plugin-host-internals.h"
#include "utility.h"
#include "aap/core/host/plugin-client-system.h"
#include "../extensions/parameters-service.h"
#include "../extensions/presets-service.h"
#include "../extensions/midi-service.h"
#include "../extensions/port-config-service.h"
#include "../extensions/gui-service.h"
#include "aap/core/host/plugin-instance.h"


#define AAP_HOST_TAG "AAP_HOST"

namespace aap
{

std::unique_ptr<PluginServiceList> plugin_service_list{nullptr};

PluginServiceList* PluginServiceList::getInstance() {
	if (!plugin_service_list)
		plugin_service_list = std::make_unique<PluginServiceList>();
	return plugin_service_list.get();
}
//-----------------------------------

PluginInformation::PluginInformation(bool isOutProcess, const char* pluginPackageName,
									 const char* pluginLocalName, const char* displayName,
									 const char* developerName, const char* versionString,
									 const char* pluginID, const char* sharedLibraryFilename,
									 const char* libraryEntrypoint, const char* metadataFullPath,
									 const char* primaryCategory, const char* uiViewFactory,
									 const char* uiActivity, const char* uiWeb)
			: is_out_process(isOutProcess),
			  plugin_package_name(pluginPackageName),
			  plugin_local_name(pluginLocalName),
			  display_name(displayName),
			  developer_name(developerName),
			  version(versionString),
			  shared_library_filename(sharedLibraryFilename),
			  library_entrypoint(libraryEntrypoint),
			  plugin_id(pluginID),
			  metadata_full_path(metadataFullPath),
			  primary_category(primaryCategory),
			  ui_view_factory(uiViewFactory),
			  ui_activity(uiActivity),
			  ui_web(uiWeb)
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

PluginExtensionInformation PluginInformation::getExtension(const char *uri) const {
	for (int i = 0, n = getNumExtensions(); i < n; i++) {
		auto ext = getExtension(i);
		if (strcmp(ext.uri.c_str(), uri) == 0)
			return ext;
	}
	return {};
}


//-----------------------------------

bool AbstractPluginBuffer::initialize(int32_t numPorts, int32_t numFrames) {
	assert(!buffers); // already allocated
	assert(!buffer_sizes); // already allocated

	num_ports = numPorts;
	num_frames = numFrames;
	buffers = (void **) calloc(numPorts, sizeof(float *)); // this part is always local
	if (!buffers)
		return false;
	buffer_sizes = (int32_t*) calloc(numPorts, sizeof(int32_t)); // this part is always local
	if (!buffer_sizes)
		return false;
	return true;
}

//-----------------------------------

int32_t ClientPluginSharedMemoryStore::allocateClientBuffer(size_t numPorts, size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_LOCAL;

	size_t commonMemSize = numFrames * sizeof(float);
	port_buffer = std::make_unique<SharedMemoryPluginBuffer>();
	assert(port_buffer);
	port_buffer->initialize(numPorts, numFrames);

	for (size_t i = 0; i < numPorts; i++) {
		auto port = instance.getPort(i);
		auto memSize = port->hasProperty(AAP_PORT_MINIMUM_SIZE) ? port->getPropertyAsInteger(AAP_PORT_MINIMUM_SIZE) :
				port->getContentType() == AAP_CONTENT_TYPE_AUDIO ? commonMemSize : defaultControllBytesPerBlock;
		int32_t fd = PluginClientSystem::getInstance()->createSharedMemory(memSize);
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		port_buffer_fds->emplace_back(fd);
		auto mapped = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!mapped)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
		port_buffer->setBuffer(i, mapped);
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

int32_t ServicePluginSharedMemoryStore::allocateServiceBuffer(std::vector<int32_t>& clientFDs, size_t numFrames, aap::PluginInstance& instance, size_t defaultControllBytesPerBlock) {
	memory_origin = PLUGIN_BUFFER_ORIGIN_REMOTE;

	size_t numPorts = clientFDs.size();
	size_t commonMemSize = numFrames * sizeof(float);
	port_buffer = std::make_unique<SharedMemoryPluginBuffer>();
	assert(port_buffer);
	if (!port_buffer->initialize(numPorts, numFrames))
		return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_LOCAL_ALLOC;

	for (size_t i = 0; i < numPorts; i++) {
		auto port = instance.getPort(i);
		auto memSize = port->hasProperty(AAP_PORT_MINIMUM_SIZE) ? port->getPropertyAsInteger(AAP_PORT_MINIMUM_SIZE) :
					   port->getContentType() == AAP_CONTENT_TYPE_AUDIO ? commonMemSize : defaultControllBytesPerBlock;
		int32_t fd = clientFDs[i];
		if (!fd)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_SHM_CREATE;
		port_buffer_fds->emplace_back(fd);
		auto mapped = mmap(nullptr, memSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (!mapped)
			return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_FAILED_MMAP;
        port_buffer->setBuffer(i, mapped);
	}

	return PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS;
}

//-----------------------------------

std::unique_ptr<StateExtensionFeature> aapxs_state{nullptr};
std::unique_ptr<PresetsExtensionFeature> aapxs_presets{nullptr};
std::unique_ptr<PortConfigExtensionFeature> aapxs_port_config{nullptr};
std::unique_ptr<MidiExtensionFeature> aapxs_midi2{nullptr};
std::unique_ptr<ParametersExtensionFeature> aapxs_parameters{nullptr};
std::unique_ptr<GuiExtensionFeature> aapxs_gui{nullptr};
std::unique_ptr<AAPXSRegistry> aapxs_registry{nullptr};

PluginHost::PluginHost(PluginListSnapshot* contextPluginList)
	: plugin_list(contextPluginList)
{
	assert(contextPluginList);

	if (aapxs_registry == nullptr)
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
		aapxs_midi2 = std::make_unique<MidiExtensionFeature>();
	aapxs_registry->add(aapxs_midi2->asPublicApi());
	// parameters
	if (aapxs_parameters == nullptr)
		aapxs_parameters = std::make_unique<ParametersExtensionFeature>();
	// gui
	if (aapxs_gui == nullptr)
		aapxs_gui = std::make_unique<GuiExtensionFeature>();
	aapxs_registry->add(aapxs_gui->asPublicApi());
}

AAPXSFeature* PluginHost::getExtensionFeature(const char* uri) {
	return aapxs_registry->getByUri(uri);
}

void PluginHost::destroyInstance(PluginInstance* instance)
{
	instances.erase(std::find(instances.begin(), instances.end(), instance));
	delete instance;
}

PluginInstance* PluginHost::getInstanceByIndex(int32_t index) {
    assert(0 <= index);
    assert(index < instances.size());
    return instances[index];
}

PluginInstance* PluginHost::getInstanceById(int32_t instanceId) {
    for (auto i: instances)
        if (i->getInstanceId() == instanceId)
            return i;
    return nullptr;
}

int32_t localInstanceIdSerial{0};

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
		aap::a_log_f(AAP_LOG_LEVEL_ERROR,  AAP_HOST_TAG, "aap::PluginHost: AAP library %s could not be loaded: %s", file.c_str(), dlerror());
		return nullptr;
	}
	auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint.length() > 0 ? entrypoint.c_str() : "GetAndroidAudioPluginFactory");
	if (factoryGetter == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_HOST_TAG, "aap::PluginHost: AAP factory entrypoint function %s was not found in %s.", entrypoint.c_str(), file.c_str());
		return nullptr;
	}
	auto pluginFactory = factoryGetter();
	if (pluginFactory == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_HOST_TAG, "aap::PluginHost: AAP factory entrypoint function %s could not instantiate a plugin.", entrypoint.c_str());
		return nullptr;
	}
	auto instance = new LocalPluginInstance(this, aapxs_registry.get(), localInstanceIdSerial++,
											descriptor, pluginFactory, sampleRate, event_midi2_input_buffer_size);
	instances.emplace_back(instance);
	return instance;
}

void PluginClient::connectToPluginService(const std::string& identifier, std::function<void(std::string&)> callback) {
	const PluginInformation *descriptor = plugin_list->getPluginInformation(identifier);
	assert (descriptor != nullptr);
	connectToPluginService(descriptor->getPluginPackageName(), descriptor->getPluginLocalName(), callback);
}
void PluginClient::connectToPluginService(const std::string& packageName, const std::string& className, std::function<void(std::string&)> callback) {
	auto service = connections->getServiceHandleForConnectedPlugin(packageName, className);
	if (service != nullptr) {
		std::string error{};
		callback(error);
	} else {
		PluginClientSystem::getInstance()->ensurePluginServiceConnected(connections, packageName, callback);
	}
}

void PluginClient::createInstanceAsync(std::string identifier, int sampleRate, bool isRemoteExplicit, std::function<void(int32_t, std::string&)>& userCallback)
{
	std::function<void(std::string&)> cb = [identifier,sampleRate,isRemoteExplicit,userCallback,this](std::string& error) {
		if (error.empty()) {
			auto result = createInstance(identifier, sampleRate, isRemoteExplicit);
			userCallback(result.value, result.error);
		}
		else
			userCallback(-1, error);
	};
	connectToPluginService(identifier, cb);
}

PluginClient::Result<int32_t> PluginClient::createInstance(std::string identifier, int sampleRate, bool isRemoteExplicit)
{
	Result<int32_t> result;
	const PluginInformation *descriptor = plugin_list->getPluginInformation(identifier);
	assert (descriptor != nullptr);

	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	auto internalCallback = [=](PluginInstance* instance, std::string error) {
		if (instance != nullptr) {
			return Result<int32_t>{instance->getInstanceId(), ""};
		}
		else
			return Result<int32_t>{-1, error};
	};
	if (isRemoteExplicit || descriptor->isOutProcess())
		return instantiateRemotePlugin(descriptor, sampleRate);
	else {
		try {
			auto instance = instantiateLocalPlugin(descriptor, sampleRate);
			return internalCallback(instance, "");
		} catch(std::exception& ex) {
			return internalCallback(nullptr, ex.what());
		}
	}
}

PluginClient::Result<int32_t> PluginClient::instantiateRemotePlugin(const PluginInformation *descriptor, int sampleRate)
{
    // We first ensure to bind the remote plugin service, and then create a plugin instance.
    //  Since binding the plugin service must be asynchronous while instancing does not have to be,
    //  we call ensureServiceConnected() first and pass the rest as its callback.
    auto internalCallback = [=](std::string error) {
        if (error.empty()) {
#if ANDROID
            auto pluginFactory = GetAndroidAudioPluginFactoryClientBridge(this);
#else
            auto pluginFactory = GetDesktopAudioPluginFactoryClientBridge(this);
#endif
            assert (pluginFactory != nullptr);
            auto instance = new RemotePluginInstance(this, aapxs_registry.get(), descriptor, pluginFactory,
													 sampleRate, event_midi2_input_buffer_size);
            instances.emplace_back(instance);
            instance->completeInstantiation();
            instance->scanParametersAndBuildList();
            return Result<int32_t>{instance->getInstanceId(), ""};
        }
        else
			return Result<int32_t>{-1, error};
    };
    auto service = connections->getServiceHandleForConnectedPlugin(descriptor->getPluginPackageName(), descriptor->getPluginLocalName());
    if (service != nullptr)
        return internalCallback("");
    else
		return internalCallback(std::string{"Plugin service is not started yet: "} + descriptor->getPluginID());
}

int32_t PluginService::createInstance(std::string identifier, int sampleRate)  {
	auto info = plugin_list->getPluginInformation(identifier);
	if (!info) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_HOST_TAG, "aap::PluginService: Plugin information was not found for: %s ", identifier.c_str());
		return -1;
	}
	auto instance = instantiateLocalPlugin(info, sampleRate);
	return instance->getInstanceId();
}

//-----------------------------------

PluginInstance::PluginInstance(const PluginInformation* pluginInformation,
							   AndroidAudioPluginFactory* loadedPluginFactory,
							   int32_t sampleRate,
							   int32_t eventMidi2InputBufferSize)
		: sample_rate(sampleRate),
		  plugin_factory(loadedPluginFactory),
		  instantiation_state(PLUGIN_INSTANTIATION_STATE_INITIAL),
		  plugin(nullptr),
		  pluginInfo(pluginInformation),
          event_midi2_input_buffer_size(eventMidi2InputBufferSize) {
	assert(pluginInformation);
	assert(loadedPluginFactory);
	assert(event_midi2_input_buffer_size > 0);
	event_midi2_input_buffer = calloc(1, event_midi2_input_buffer_size);
    event_midi2_input_buffer_merged = calloc(1, event_midi2_input_buffer_size);
}

PluginInstance::~PluginInstance() {
	instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
	if (plugin != nullptr)
		plugin_factory->release(plugin_factory, plugin);
	plugin = nullptr;
	delete shared_memory_store;
	if (event_midi2_input_buffer)
		free(event_midi2_input_buffer);
	if (event_midi2_input_buffer_merged)
		free(event_midi2_input_buffer_merged);
}

aap_buffer_t* PluginInstance::getAudioPluginBuffer() {
	return shared_memory_store->getAudioPluginBuffer();
}

void PluginInstance::completeInstantiation()
{
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_INITIAL) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_HOST_TAG,
                     "Unexpected call to completeInstantiation() at state: %d (instanceId: %d)",
                     instantiation_state, instance_id);
        return;
    }

	AndroidAudioPluginHost* asPluginAPI = getHostFacadeForCompleteInstantiation();
	plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), sample_rate, asPluginAPI);
	assert(plugin);

	instantiation_state = PLUGIN_INSTANTIATION_STATE_UNPREPARED;
}

//----

RemotePluginInstance::RemotePluginInstance(PluginClient* client,
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

void RemotePluginInstance::configurePorts() {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_HOST_TAG,
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

void PluginInstance::setupPortConfigDefaults() {
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

void PluginInstance::scanParametersAndBuildList() {
	assert(!cached_parameters);

	auto ext = (aap_parameters_extension_t*) plugin->get_extension(plugin, AAP_PARAMETERS_EXTENSION_URI);
	if (!ext)
		return;

	// if parameters extension does not exist, do not populate cached parameters list.
	// (The empty list means no parameters in metadata either.)
	cached_parameters = std::make_unique<std::vector<ParameterInformation>>();

	for (auto i = 0, n = ext->get_parameter_count(ext, plugin); i < n; i++) {
		auto para = ext->get_parameter(ext, plugin, i);
		ParameterInformation p{para->stable_id, para->display_name, para->min_value, para->max_value, para->default_value};
		if (ext->get_enumeration_count && ext->get_enumeration) {
			for (auto e = 0, en = ext->get_enumeration_count(ext, plugin, i); e < en; e++) {
				auto pe = ext->get_enumeration(ext, plugin, i, e);
				ParameterInformation::Enumeration eDef{e, pe->value, pe->name};
				p.addEnumeration(eDef);
			}
		}
		cached_parameters->emplace_back(p);
	}
}

void PluginInstance::addEventUmpInput(void *input, int32_t size) {
	const std::lock_guard<NanoSleepLock> lock{event_input_buffer_mutex};
	if (event_midi2_input_buffer_offset + size > event_midi2_input_buffer_size)
		return;
	memcpy((uint8_t *) event_midi2_input_buffer + event_midi2_input_buffer_offset,
		   input, size);
	event_midi2_input_buffer_offset += size;
}

void PluginInstance::process(int32_t frameCount, int32_t timeoutInNanoseconds)  {
	struct timespec timeSpecBegin{}, timeSpecEnd{};
#if ANDROID
	if (ATrace_isEnabled()) {
		ATrace_beginSection(this->pluginInfo->isOutProcess() ? remote_trace_name : local_trace_name);
		clock_gettime(CLOCK_REALTIME, &timeSpecBegin);
	}
#endif

	if (std::unique_lock<NanoSleepLock> tryLock(event_input_buffer_mutex, std::try_to_lock); tryLock.owns_lock()) {
		merge_event_inputs(event_midi2_input_buffer_merged, event_midi2_input_buffer_size,
						   event_midi2_input_buffer, event_midi2_input_buffer_offset,
						   getAudioPluginBuffer(), this);
		event_midi2_input_buffer_offset = 0;
	}
	plugin->process(plugin, getAudioPluginBuffer(), frameCount, timeoutInNanoseconds);

#if ANDROID
	if (ATrace_isEnabled()) {
		clock_gettime(CLOCK_REALTIME, &timeSpecEnd);
		ATrace_setCounter(this->pluginInfo->isOutProcess() ? remote_trace_name : local_trace_name,
						  (timeSpecEnd.tv_sec - timeSpecBegin.tv_sec) * 1000000000 + timeSpecEnd.tv_nsec - timeSpecBegin.tv_nsec);
		ATrace_endSection();
	}
#endif
}

AndroidAudioPluginHost* RemotePluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = nullptr; // we shouldn't need it.
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

void RemotePluginInstance::prepare(int frameCount) {
    if (instantiation_state != PLUGIN_INSTANTIATION_STATE_UNPREPARED) {
        aap::a_log_f(AAP_LOG_LEVEL_ERROR, AAP_HOST_TAG,
                     "Unexpected call to prepare() at state: %d (instanceId: %d)",
                     instantiation_state, instance_id);
        return;
    }

    auto numPorts = getNumPorts();
    auto shm = dynamic_cast<aap::ClientPluginSharedMemoryStore*>(getSharedMemoryStore());
    auto code = shm->allocateClientBuffer(numPorts, frameCount, *this, DEFAULT_CONTROL_BUFFER_SIZE);
    if (code != aap::PluginSharedMemoryStore::PluginMemoryAllocatorResult::PLUGIN_MEMORY_ALLOCATOR_SUCCESS) {
        aap::a_log(AAP_LOG_LEVEL_ERROR, AAP_HOST_TAG, aap::PluginSharedMemoryStore::getMemoryAllocationErrorMessage(code));
    }

    plugin->prepare(plugin, getAudioPluginBuffer());
    instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
}

void* RemotePluginInstance::getRemoteWebView()  {
#if ANDROID
	return AAPJniFacade::getInstance()->getRemoteWebView(client, this);
#else
	// cannot do anything
	return nullptr;
#endif
}

void RemotePluginInstance::prepareSurfaceControlForRemoteNativeUI() {
#if ANDROID
	if (!native_ui_controller)
		native_ui_controller = std::make_unique<RemotePluginNativeUIController>(this);
#else
	// cannot do anything
#endif
}

void* RemotePluginInstance::getRemoteNativeView()  {
#if ANDROID
	assert(native_ui_controller);
	return AAPJniFacade::getInstance()->getRemoteNativeView(client, this);
#else
	// cannot do anything
	return nullptr;
#endif
}

void RemotePluginInstance::connectRemoteNativeView(int32_t width, int32_t height)  {
#if ANDROID
	assert(native_ui_controller);
	AAPJniFacade::getInstance()->connectRemoteNativeView(client, this, width, height);
#else
	// cannot do anything
#endif
}

//----

RemotePluginInstance::RemotePluginNativeUIController::RemotePluginNativeUIController(RemotePluginInstance* owner) {
	handle = AAPJniFacade::getInstance()->createSurfaceControl();
}

RemotePluginInstance::RemotePluginNativeUIController::~RemotePluginNativeUIController() {
	AAPJniFacade::getInstance()->disposeSurfaceControl(handle);
}

void RemotePluginInstance::RemotePluginNativeUIController::show() {
	AAPJniFacade::getInstance()->showSurfaceControlView(handle);
}

void RemotePluginInstance::RemotePluginNativeUIController::hide() {
	AAPJniFacade::getInstance()->hideSurfaceControlView(handle);
}

//----

LocalPluginInstance::LocalPluginInstance(PluginHost *host,
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
	shared_memory_store = new ServicePluginSharedMemoryStore();
    instance_id = instanceId;
}

AndroidAudioPluginHost* LocalPluginInstance::getHostFacadeForCompleteInstantiation() {
    plugin_host_facade.context = this;
    plugin_host_facade.get_extension = internalGetExtension;
	plugin_host_facade.request_process = internalRequestProcess;
    return &plugin_host_facade;
}

// plugin-info host extension implementation.
static uint32_t plugin_info_port_get_index(aap_plugin_info_port_t* port) { return ((PortInformation*) port->context)->getIndex(); }
static const char* plugin_info_port_get_name(aap_plugin_info_port_t* port) { return ((PortInformation*) port->context)->getName(); }
static aap_content_type plugin_info_port_get_content_type(aap_plugin_info_port_t* port) { return (aap_content_type) ((PortInformation*) port->context)->getContentType(); }
static aap_port_direction plugin_info_port_get_direction(aap_plugin_info_port_t* port) { return (aap_port_direction) ((PortInformation*) port->context)->getPortDirection(); }

static aap_plugin_info_port_t plugin_info_get_port(aap_plugin_info_t* plugin, uint32_t index) {
	auto port = ((LocalPluginInstance*) plugin->context)->getPort(index);
	return aap_plugin_info_port_t{(void *) port,
								  plugin_info_port_get_index,
								  plugin_info_port_get_name,
								  plugin_info_port_get_content_type,
								  plugin_info_port_get_direction};
}

static const char* plugin_info_get_plugin_package_name(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getPluginPackageName().c_str(); }
static const char* plugin_info_get_plugin_local_name(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getPluginLocalName().c_str(); }
static const char* plugin_info_get_display_name(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getDisplayName().c_str(); }
static const char* plugin_info_get_developer_name(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getDeveloperName().c_str(); }
static const char* plugin_info_get_version(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getVersion().c_str(); }
static const char* plugin_info_get_primary_category(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getPrimaryCategory().c_str(); }
static const char* plugin_info_get_identifier_string(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getStrictIdentifier().c_str(); }
static const char* plugin_info_get_plugin_id(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getPluginInformation()->getPluginID().c_str(); }
static uint32_t plugin_info_get_port_count(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getNumPorts(); }
static uint32_t plugin_info_get_parameter_count(aap_plugin_info_t* plugin) { return ((LocalPluginInstance*) plugin->context)->getNumParameters(); }

static uint32_t plugin_info_parameter_get_id(aap_plugin_info_parameter_t* parameter) { return ((ParameterInformation*) parameter->context)->getId(); }
static const char* plugin_info_parameter_get_name(aap_plugin_info_parameter_t* parameter) { return ((ParameterInformation*) parameter->context)->getName(); }
static float plugin_info_parameter_get_min_value(aap_plugin_info_parameter_t* parameter) { return (aap_content_type) ((ParameterInformation*) parameter->context)->getMinimumValue(); }
static float plugin_info_parameter_get_max_value(aap_plugin_info_parameter_t* parameter) { return (aap_content_type) ((ParameterInformation*) parameter->context)->getMaximumValue(); }
static float plugin_info_parameter_get_default_value(aap_plugin_info_parameter_t* parameter) { return (aap_content_type) ((ParameterInformation*) parameter->context)->getDefaultValue(); }

static aap_plugin_info_parameter_t plugin_info_get_parameter(aap_plugin_info_t* plugin, uint32_t index) {
	auto para = ((LocalPluginInstance*) plugin->context)->getParameter(index);
	return aap_plugin_info_parameter_t{(void *) para,
									   plugin_info_parameter_get_id,
									   plugin_info_parameter_get_name,
									   plugin_info_parameter_get_min_value,
									   plugin_info_parameter_get_max_value,
									   plugin_info_parameter_get_default_value};
}

aap_plugin_info_t LocalPluginInstance::get_plugin_info(aap_host_plugin_info_extension_t* ext, AndroidAudioPluginHost* host, const char* pluginId) {
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

void LocalPluginInstance::notify_parameters_changed(aap_host_parameters_extension_t* ext, AndroidAudioPluginHost *host,
                                                AndroidAudioPlugin *plugin) {
    assert(false); // FIXME: implement
}

void LocalPluginInstance::requestProcessToHost() {
	if (process_requested)
		return;
	process_requested = true;
	((PluginService*) host)->requestProcessToHost(instance_id);
}

//----

AAPXSClientInstance* RemoteAAPXSManager::setupAAPXSInstance(AAPXSFeature *feature, int32_t dataSize) {
	const char* uri = aapxsClientInstances.addOrGetUri(feature->uri);
	assert (aapxsClientInstances.get(uri) == nullptr);
	if (dataSize < 0)
		dataSize = feature->shared_memory_size;
	aapxsClientInstances.add(uri, std::make_unique<AAPXSClientInstance>(AAPXSClientInstance{owner, uri, owner->instance_id, nullptr, dataSize}));
	auto ret = aapxsClientInstances.get(uri);
	ret->extension_message = staticSendExtensionMessage;
	return ret;
}

void RemoteAAPXSManager::staticSendExtensionMessage(AAPXSClientInstance* clientInstance, int32_t opcode) {
	auto thisObj = (RemotePluginInstance*) clientInstance->host_context;
	thisObj->sendExtensionMessage(clientInstance->uri, opcode);
}
} // namespace
