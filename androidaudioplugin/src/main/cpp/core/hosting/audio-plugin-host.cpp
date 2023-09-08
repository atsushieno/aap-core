
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
#include "aap/core/host/plugin-client-system.h"
#include "../extensions/parameters-service.h"
#include "../extensions/presets-service.h"
#include "../extensions/midi-service.h"
#include "../extensions/port-config-service.h"
#include "aap/core/host/plugin-instance.h"
#include "aap/core/AAPXSMidi2Processor.h"


#define LOG_TAG "AAP_HOST"

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
	port_buffer = std::make_unique<SharedMemoryPluginBuffer>(&instance);
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
	port_buffer = std::make_unique<SharedMemoryPluginBuffer>(&instance);
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
std::unique_ptr<AAPXSRegistry> standard_aapxs_registry{nullptr};

void initializeStandardAAPXSRegistry() {
    auto aapxs_registry = std::make_unique<AAPXSRegistry>();

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
    aapxs_registry->add(aapxs_parameters->asPublicApi());

    aapxs_registry->freeze();

    standard_aapxs_registry = std::move(aapxs_registry);
}

PluginHost::PluginHost(PluginListSnapshot* contextPluginList, AAPXSRegistry* aapxsRegistry)
	: plugin_list(contextPluginList)
{
	assert(contextPluginList);

	if (standard_aapxs_registry == nullptr)
        initializeStandardAAPXSRegistry();
    aapxs_registry = aapxsRegistry ? aapxsRegistry : standard_aapxs_registry.get();
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
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginHost: AAP library %s could not be loaded: %s", file.c_str(), dlerror());
		return nullptr;
	}
	auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint.length() > 0 ? entrypoint.c_str() : "GetAndroidAudioPluginFactory");
	if (factoryGetter == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginHost: AAP factory entrypoint function %s was not found in %s.", entrypoint.c_str(), file.c_str());
		return nullptr;
	}
	auto pluginFactory = factoryGetter();
	if (pluginFactory == nullptr) {
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginHost: AAP factory entrypoint function %s could not instantiate a plugin.", entrypoint.c_str());
		return nullptr;
	}
	auto instance = new LocalPluginInstance(this, aapxs_registry, localInstanceIdSerial++,
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
    auto internalCallback = [this,descriptor,sampleRate](std::string error) {
        if (error.empty()) {
#if ANDROID
            auto pluginFactory = GetAndroidAudioPluginFactoryClientBridge(this);
#else
            auto pluginFactory = GetDesktopAudioPluginFactoryClientBridge(this);
#endif
            assert (pluginFactory != nullptr);
            auto instance = new RemotePluginInstance(this, aapxs_registry, descriptor, pluginFactory,
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
		aap::a_log_f(AAP_LOG_LEVEL_ERROR, LOG_TAG, "aap::PluginService: Plugin information was not found for: %s ", identifier.c_str());
		return -1;
	}
	auto instance = instantiateLocalPlugin(info, sampleRate);
	return instance->getInstanceId();
}

} // namespace
