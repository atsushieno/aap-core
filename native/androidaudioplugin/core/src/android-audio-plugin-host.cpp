#include "../include/aap/android-audio-plugin-host.hpp"
#include <vector>
#include <tinyxml2.h>

namespace aap
{

void aap_parse_plugin_descriptor_into(const char* xmlfile, std::vector<PluginInformation*>& plugins)
{
    tinyxml2::XMLDocument doc;
    auto error = doc.LoadFile(xmlfile);
    if (error != tinyxml2::XMLError::XML_SUCCESS)
        return;
    auto pluginsElement = doc.FirstChildElement("plugins");
    for (auto pluginElement = pluginsElement->FirstChildElement("plugin");
            pluginElement != nullptr;
            pluginElement = pluginElement->NextSiblingElement("plugin")) {
        auto plugin = new PluginInformation(false,
                pluginElement->Attribute("name"),
                pluginElement->Attribute("manufacturer"),
                pluginElement->Attribute("version"),
                pluginElement->Attribute("unique-id"),
                pluginElement->Attribute("library"),
                pluginElement->Attribute("entrypoint"));
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
            plugin->addPort(new PortInformation(portName, content, direction));
        }
        plugins.push_back(plugin);
    }
}

PluginInformation** PluginInformation::ParsePluginDescriptor(const char* xmlfile) { return aap_parse_plugin_descriptor(xmlfile); }

PluginInformation** aap_parse_plugin_descriptor(const char* xmlfile)
{
    std::vector<PluginInformation*> plugins;
    aap_parse_plugin_descriptor_into(xmlfile, plugins);
    PluginInformation** ret = (PluginInformation**) calloc(sizeof(PluginInformation*), plugins.size() + 1);
    for(size_t i = 0; i < plugins.size(); i++)
        ret[i] = plugins[i];
    ret[plugins.size()] = nullptr;
    return ret;
}

PluginHost::PluginHost(const PluginInformation* const* pluginDescriptors)
{
	int n = 0;
	while (pluginDescriptors[n])
		n++;
	for (int i = 0; i < n; i++)
		plugin_descriptors.push_back(pluginDescriptors[i]);
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
	const char *file = descriptor->getLocalPluginSharedLibrary().data();
	const char *entrypoint = descriptor->getLocalPluginLibraryEntryPoint().data();
	auto dl = dlopen(file && file[0] ? file : "libandroidaudioplugin.so", RTLD_LAZY);
	assert (dl != nullptr);
	auto factoryGetter = (aap_factory_t) dlsym(dl, entrypoint && entrypoint[0] ? entrypoint : "GetAndroidAudioPluginFactory");
	assert (factoryGetter != nullptr);
	auto pluginFactory = factoryGetter();
	assert (pluginFactory != nullptr);
	return new PluginInstance(this, descriptor, pluginFactory);
}

PluginInstance* PluginHost::instantiateRemotePlugin(const PluginInformation *descriptor)
{
	auto dl = dlopen("libandroidaudioplugin.so", RTLD_LAZY);
	assert (dl != nullptr);
	auto factoryGetter = (aap_factory_t) dlsym(dl, "GetAndroidAudioPluginFactoryClientBridge");
	assert (factoryGetter != nullptr);
	auto pluginFactory = factoryGetter();
	assert (pluginFactory != nullptr);
	return new PluginInstance(this, descriptor, pluginFactory);
}

} // namespace
