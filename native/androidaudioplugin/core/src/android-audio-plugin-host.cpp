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

void aap_parse_plugin_descriptor_into(const char* containerIdentifier, const char* xmlfile, std::vector<PluginInformation*>& plugins)
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
                containerIdentifier ? containerIdentifier : "",
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

std::vector<PluginInformation*> PluginInformation::parsePluginDescriptor(const char * containerIdentifier, const char* xmlfile)
{
	std::vector<PluginInformation*> plugins{};
	aap_parse_plugin_descriptor_into(containerIdentifier, xmlfile, plugins);
	return plugins;
}

PluginHost::PluginHost()
{
	auto pal = getPluginHostPAL();
	updateKnownPlugins(pal->getInstalledPlugins());
}

void PluginHost::updateKnownPlugins(std::vector<PluginInformation*> pluginDescriptors)
{
	for (auto entry : pluginDescriptors)
		plugin_descriptors.emplace_back(entry);
}

bool PluginHost::isPluginAlive (const char *identifier) 
{
	auto desc = getPluginDescriptor(identifier);
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

bool PluginHost::isPluginUpToDate (const char *identifier, long lastInfoUpdated)
{
	auto desc = getPluginDescriptor(identifier);
	if (!desc)
		return false;

	return desc->getLastInfoUpdateTime() <= lastInfoUpdated;
}

void PluginHost::destroyPlugin(PluginInstance* instance)
{
    for (AndroidAudioPluginExtension* ext : instance->extensions) {
        if (strcmp(ext->uri, aap::SharedMemoryExtension::URI) == 0) {
            if (ext->data != nullptr)
                delete (aap::SharedMemoryExtension*) ext->data;
        }
        delete ext;
    }
	delete instance;
}

char const* SharedMemoryExtension::URI = "aap-extension:org.androidaudioplugin.SharedMemoryExtension";

PluginInstance* PluginHost::instantiatePlugin(const char *identifier)
{
	const PluginInformation *descriptor = getPluginDescriptor(identifier);
	assert (descriptor);

	// For local plugins, they can be directly loaded using dlopen/dlsym.
	// For remote plugins, the connection has to be established through binder.
	if (descriptor->isOutProcess())
		return instantiateRemotePlugin(descriptor);
	else
		return instantiateLocalPlugin(descriptor);
}

PluginInstance* PluginHost::instantiateLocalPlugin(const PluginInformation *descriptor)
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
	return new PluginInstance(this, descriptor, pluginFactory);
}

PluginInstance* PluginHost::instantiateRemotePlugin(const PluginInformation *descriptor)
{
	dlerror(); // clean up any previous error state
	auto dl = dlopen("libandroidaudioplugin.so", RTLD_LAZY);
	assert (dl != nullptr);
	auto factoryGetter = (aap_factory_t) dlsym(dl, "GetAndroidAudioPluginFactoryClientBridge");
	assert (factoryGetter != nullptr);
	auto pluginFactory = factoryGetter();
	assert (pluginFactory != nullptr);
	auto instance = new PluginInstance(this, descriptor, pluginFactory);
	auto ext = new AndroidAudioPluginExtension();
	ext->uri = aap::SharedMemoryExtension::URI;
	ext->data = new aap::SharedMemoryExtension();
	instance->extensions.emplace_back (ext);
	return instance;
}

} // namespace
