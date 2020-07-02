#include "../include/aap/android-audio-plugin-host.hpp"
#include "../include/aap/logging.h"
#include <vector>
#include <tinyxml2.h>

#if ANDROID
#include <jni.h>
#include "aidl/org/androidaudioplugin/BnAudioPluginInterface.h"
#include "aidl/org/androidaudioplugin/BpAudioPluginInterface.h"
#endif

namespace aap
{

// plugin globals ---------------------

void aap_parse_plugin_descriptor_into(const char* pluginPackageName, const char* pluginLocalName, const char* xmlfile, std::vector<PluginInformation*>& plugins)
{
    tinyxml2::XMLDocument doc;
    auto error = doc.LoadFile(xmlfile);
    if (error != tinyxml2::XMLError::XML_SUCCESS)
        return;
    auto pluginsElement = doc.FirstChildElement("plugins");
    for (auto pluginElement = pluginsElement->FirstChildElement("plugin");
            pluginElement != nullptr;
            pluginElement = pluginElement->NextSiblingElement("plugin")) {
        auto name = pluginElement->Attribute("name");
        auto manufacturer = pluginElement->Attribute("manufacturer");
        auto version = pluginElement->Attribute("version");
        auto uid = pluginElement->Attribute("unique-id");
        auto library = pluginElement->Attribute("library");
        auto entrypoint = pluginElement->Attribute("entrypoint");
        auto plugin = new PluginInformation(false,
        		pluginPackageName ? pluginPackageName : "",
        		pluginLocalName ? pluginLocalName : "",
                name ? name : "",
                manufacturer ? manufacturer : "",
                version ? version : "",
                uid ? uid : "",
                library ? library : "",
                entrypoint ? entrypoint : "");
        auto portsElement = pluginElement->FirstChildElement("ports");
        for (auto portElement = portsElement->FirstChildElement("port");
                portElement != nullptr;
                portElement = portElement->NextSiblingElement("port")) {
            auto portName = portElement->Attribute("name");
            auto contentString = portElement->Attribute("content");
            ContentType content =
                    !strcmp(contentString, "audio") ? ContentType::AAP_CONTENT_TYPE_AUDIO :
                    !strcmp(contentString, "midi") ? ContentType::AAP_CONTENT_TYPE_MIDI :
                    ContentType::AAP_CONTENT_TYPE_UNDEFINED;
            auto directionString = portElement->Attribute("direction");
            PortDirection  direction = !strcmp(directionString, "input") ? PortDirection::AAP_PORT_DIRECTION_INPUT : PortDirection::AAP_PORT_DIRECTION_OUTPUT;
            auto defaultValue = portElement->Attribute("default");
			auto minimumValue = portElement->Attribute("minimum");
			auto maximumValue = portElement->Attribute("maximum");
            if (defaultValue != nullptr || minimumValue != nullptr || maximumValue != nullptr)
				plugin->addPort(new PortInformation(portName, content, direction,
						atof(defaultValue), atof(minimumValue), atof(maximumValue)));
            else
            	plugin->addPort(new PortInformation(portName, content, direction));
        }
        plugins.push_back(plugin);
    }
}

//-----------------------------------

std::vector<PluginInformation*> PluginInformation::parsePluginMetadataXml(const char * pluginPackageName, const char * pluginLocalName, const char* xmlfile)
{
	std::vector<PluginInformation*> plugins{};
	aap_parse_plugin_descriptor_into(pluginPackageName, pluginLocalName, xmlfile, plugins);
	return plugins;
}

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

bool PluginHostManager::isPluginAlive (const char *identifier)
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

bool PluginHostManager::isPluginUpToDate (const char *identifier, int64_t lastInfoUpdatedTimeInMilliseconds)
{
	auto desc = getPluginInformation(identifier);
	if (!desc)
		return false;

	return desc->getLastInfoUpdateTime() <= lastInfoUpdatedTimeInMilliseconds;
}

char const* SharedMemoryExtension::URI = "aap-extension:org.androidaudioplugin.SharedMemoryExtension";

int PluginHost::createInstance(const char *identifier, int sampleRate)
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
	auto factoryGetter = (aap_factory_t) dlsym(dl, "GetAndroidAudioPluginFactoryClientBridge");
	assert (factoryGetter != nullptr);
	auto pluginFactory = factoryGetter();
	assert (pluginFactory != nullptr);
	auto instance = new PluginInstance(this, descriptor, pluginFactory, sampleRate);
	AndroidAudioPluginExtension ext{aap::SharedMemoryExtension::URI, 0, new aap::SharedMemoryExtension()};
	instance->addExtension (ext);
	return instance;
}

} // namespace
