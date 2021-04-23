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

void PluginHost::destroyInstance(PluginInstance* instance)
{
	auto shmExt = instance->getSharedMemoryExtension();
	if (shmExt != nullptr)
		delete shmExt;
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

int PluginHost::createInstance(std::string identifier, int sampleRate)
{
	const PluginInformation *descriptor = manager->getPluginInformation(identifier);
	assert (descriptor != nullptr);

	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	auto instance = descriptor->isOutProcess() ?
			instantiateRemotePlugin(descriptor, sampleRate) :
			instantiateLocalPlugin(descriptor, sampleRate);
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

PluginInstance* PluginHost::instantiateRemotePlugin(const PluginInformation *descriptor, int sampleRate)
{
	dlerror(); // clean up any previous error state
	auto dl = dlopen("libandroidaudioplugin.so", RTLD_LAZY);
	assert (dl != nullptr);
	auto factoryGetter = (aap_factory_t) dlsym(dl, getPluginHostPAL()->getRemotePluginEntrypoint().c_str());
	assert (factoryGetter != nullptr);
	auto pluginFactory = factoryGetter();
	assert (pluginFactory != nullptr);
	auto instance = new PluginInstance(this, descriptor, pluginFactory, sampleRate);
	AndroidAudioPluginExtension ext{AAP_SHARED_MEMORY_EXTENSION_URI, 0, new aap::SharedMemoryExtension()};
	instance->addExtension (ext);
	return instance;
}

} // namespace
