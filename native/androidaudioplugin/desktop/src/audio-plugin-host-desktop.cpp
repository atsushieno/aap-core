
#include <string>
#include <vector>
#include <dirent.h>
#include <libgen.h>
#include <sys/utsname.h>
#include "aap/audio-plugin-host.h"
#include "audio-plugin-host-desktop.h"
#include <tinyxml2.h>

namespace aap
{

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
                                            entrypoint ? entrypoint : "",
                                            xmlfile);
        auto portsElement = pluginElement->FirstChildElement("ports");
        for (auto portElement = portsElement->FirstChildElement("port");
             portElement != nullptr;
             portElement = portElement->NextSiblingElement("port")) {
            auto portName = portElement->Attribute("name");
            auto contentString = portElement->Attribute("content");
            ContentType content =
                    !strcmp(contentString, "audio") ? ContentType::AAP_CONTENT_TYPE_AUDIO :
                    !strcmp(contentString, "midi") ? ContentType::AAP_CONTENT_TYPE_MIDI :
                    !strcmp(contentString, "midi2") ? ContentType::AAP_CONTENT_TYPE_MIDI2 :
                    ContentType::AAP_CONTENT_TYPE_UNDEFINED;
            auto directionString = portElement->Attribute("direction");
            PortDirection  direction = !strcmp(directionString, "input") ? PortDirection::AAP_PORT_DIRECTION_INPUT : PortDirection::AAP_PORT_DIRECTION_OUTPUT;
            auto defaultValue = portElement->Attribute("default");
            auto minimumValue = portElement->Attribute("minimum");
            auto maximumValue = portElement->Attribute("maximum");
            auto nativePort = new PortInformation(portName, content, direction);
            if (defaultValue != nullptr || minimumValue != nullptr || maximumValue != nullptr) {
                nativePort->setPropertyValueString(AAP_PORT_DEFAULT, defaultValue);
                nativePort->setPropertyValueString(AAP_PORT_MINIMUM, minimumValue);
                nativePort->setPropertyValueString(AAP_PORT_MAXIMUM, maximumValue);
            }
            plugin->addPort(nativePort);
        }
        plugins.push_back(plugin);
    }
}

GenericDesktopPluginHostPAL pal_instance{};

std::vector<std::string> GenericDesktopPluginHostPAL::getPluginPaths() {
    if (cached_search_paths.size() > 0)
        return cached_search_paths;

    auto aappath = getenv("AAP_PLUGIN_PATH");
    if (aappath != nullptr) {
        std::string ap{aappath};
        int idx = 0;
        while (true) {
            int si = ap.find_first_of(sep, idx);
            if (si < 0) {
                if (idx < ap.size())
                    cached_search_paths.emplace_back(std::string(ap.substr(idx, ap.size())));
                break;
            }
            cached_search_paths.emplace_back(std::string(ap.substr(idx, si)));
            idx = si + 1;
        }
        return cached_search_paths;
    }

    struct utsname un{};
    uname(&un);
#ifdef WINDOWS
    cached_search_paths.emplace_back("c:\\AAP Plugins");
cached_search_paths.emplace_back("c:\\Program Files[\\AAP Plugins");
#else
    if (strcmp(un.sysname, "Darwin") == 0) {
        cached_search_paths.emplace_back(std::string(getenv("HOME")) + "/Library/Audio/Plug-ins/AAP");
        cached_search_paths.emplace_back("/Library/Audio/Plug-ins/AAP");
    } else {
        cached_search_paths.emplace_back(std::string(getenv("HOME")) + "/.local/lib/aap");
        cached_search_paths.emplace_back("/usr/local/lib/aap");
        cached_search_paths.emplace_back("/usr/lib/aap");
    }
#endif

    return cached_search_paths;
}

PluginHostPAL* getPluginHostPAL()
{
    return &pal_instance;
}

} // namespace aap
