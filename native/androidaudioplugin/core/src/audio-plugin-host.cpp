#include <sys/stat.h>
#include "../include/aap/audio-plugin-host.h"
#include "../include/aap/logging.h"
#include <vector>

#if ANDROID
#include <jni.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#endif

namespace aap
{

//-----------------------------------

SharedMemoryExtension::SharedMemoryExtension() {
	port_buffer_fds = std::make_unique<std::vector<int32_t>>();
	extension_fds = std::make_unique<std::vector<int32_t>>();
}

//-----------------------------------

void PluginHost::destroyInstance(PluginInstance* instance)
{
	delete instance;
}

PluginHostManager::PluginHostManager()
{
	auto pal = getPluginHostPAL();
	updateKnownPlugins(pal->getInstalledPlugins());
}

void PluginHostManager::updateKnownPlugins(std::vector<PluginInformation*> pluginDescriptors)
{
	for (auto entry : pluginDescriptors)
		plugin_infos.emplace_back(entry);
}

bool PluginHostManager::isPluginAlive (std::string identifier)
{
	auto desc = getPluginInformation(identifier);
	if (!desc)
		return false;

	if (desc->isOutProcess()) {
		// TODO: implement healthcheck
	} else {
		// assets won't be removed
	}

	// need more validation?
	
	return true;
}

bool PluginHostManager::isPluginUpToDate (std::string identifier, int64_t lastInfoUpdatedTimeInMilliseconds)
{
	auto desc = getPluginInformation(identifier);
	if (!desc)
		return false;

	return desc->getLastInfoUpdateTime() <= lastInfoUpdatedTimeInMilliseconds;
}

int PluginHost::createInstance(std::string identifier, int sampleRate, bool isRemoteExplicit)
{
	const PluginInformation *descriptor = manager->getPluginInformation(identifier);
	assert (descriptor != nullptr);

	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	auto instance = isRemoteExplicit || descriptor->isOutProcess() ?
			instantiateRemotePlugin(descriptor, sampleRate) :
			instantiateLocalPlugin(descriptor, sampleRate);
	assert(instance != nullptr);
	instances.emplace_back(instance);
	return instances.size() - 1;
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
		aprintf("AAP library %s could not be loaded.\n", file.c_str());
		return nullptr;
	}
	auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint.length() > 0 ? entrypoint.c_str() : "GetAndroidAudioPluginFactory");
	if (factoryGetter == nullptr) {
		aprintf("AAP factory %s was not found in %s.\n", entrypoint.c_str(), file.c_str());
		return nullptr;
	}
	auto pluginFactory = factoryGetter();
	if (pluginFactory == nullptr) {
		aprintf("AAP factory %s could not instantiate a plugin.\n", entrypoint.c_str());
		return nullptr;
	}
	return new PluginInstance(this, descriptor, pluginFactory, sampleRate);
}

extern "C" AndroidAudioPluginFactory* (GetAndroidAudioPluginFactoryClientBridge)();
extern "C" AndroidAudioPluginFactory* (GetDesktopAudioPluginFactoryClientBridge)();

SharedMemoryExtension shmExt{};

PluginInstance* PluginHost::instantiateRemotePlugin(const PluginInformation *descriptor, int sampleRate)
{
#if ANDROID
	auto factoryGetter = GetAndroidAudioPluginFactoryClientBridge;
#else
	auto factoryGetter = GetDesktopAudioPluginFactoryClientBridge;
#endif
	assert (factoryGetter != nullptr);
	auto pluginFactory = factoryGetter();
	assert (pluginFactory != nullptr);
	auto instance = new PluginInstance(this, descriptor, pluginFactory, sampleRate);
	AndroidAudioPluginExtension ext;
	ext.uri = AAP_SHARED_MEMORY_EXTENSION_URI;
	ext.transmit_size = sizeof(SharedMemoryExtension);
	ext.data = &shmExt;
	instance->addExtension (ext);
	return instance;
}

PluginExtension::PluginExtension(AndroidAudioPluginExtension src) {
	uri.reset(strdup(src.uri));
	auto dataMem = calloc(1, src.transmit_size);
	memcpy(dataMem, src.data, src.transmit_size);
	data.reset((uint8_t*) dataMem);
	dataSize = src.transmit_size;
}

AndroidAudioPluginExtension PluginExtension::asTransient() const {
	AndroidAudioPluginExtension ret;
	ret.uri = uri.get();
	ret.data = data.get();
	ret.transmit_size = dataSize;
	return ret;
}

} // namespace
