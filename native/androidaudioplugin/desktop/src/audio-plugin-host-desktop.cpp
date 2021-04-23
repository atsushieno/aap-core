
#include <string>
#include <vector>
#include <dirent.h>
#include <libgen.h>
#include <sys/utsname.h>
#include "aap/audio-plugin-host.h"
#include "audio-plugin-host-desktop.h"
#include <libxml/tree.h>
#include <libxml/parser.h>


namespace aap
{

// FIXME: this is super hacky code
#define XC(ptr) (const xmlChar*)(ptr)
#define C(ptr) (const char*)(ptr)
void aap_parse_plugin_descriptor_into(const char* pluginPackageName, const char* pluginLocalName, const char* xmlfile, std::vector<PluginInformation*>& plugins)
{
    auto xmlParserCtxt = xmlNewParserCtxt();
    auto xmlDoc = xmlCtxtReadFile(xmlParserCtxt, xmlfile, nullptr, XML_PARSE_NOBLANKS | XML_PARSE_NOCDATA);
    if (xmlDoc) {
        auto root = xmlDocGetRootElement(xmlDoc);
        if (!strcmp(C(root->name), "plugins") && root->ns == nullptr) {
            for (xmlNode* rootChild = root->children; rootChild; rootChild = rootChild->next) {
                if (rootChild->type != XML_ELEMENT_NODE ||
                    strcmp(C(rootChild->name), "plugin") ||
                    rootChild->ns != nullptr)
                    continue;
                auto pluginElem = rootChild;
                auto name = std::unique_ptr<xmlChar>(xmlGetNsProp(pluginElem, XC("name"), nullptr));
                auto manufacturer = std::unique_ptr<xmlChar>(xmlGetNsProp(pluginElem, XC("manufacturer"), nullptr));
                auto version = std::unique_ptr<xmlChar>(xmlGetNsProp(pluginElem, XC("version"), nullptr));
                auto uid = std::unique_ptr<xmlChar>(xmlGetNsProp(pluginElem, XC("unique-id"), nullptr));
                auto library = std::unique_ptr<xmlChar>(xmlGetNsProp(pluginElem, XC("library"), nullptr));
                auto entrypoint = std::unique_ptr<xmlChar>(xmlGetNsProp(pluginElem, XC("entrypoint"), nullptr));
                auto category = std::unique_ptr<xmlChar>(xmlGetNsProp(pluginElem, XC("category"), nullptr));
                auto plugin = new PluginInformation(false,
                                            pluginPackageName ? pluginPackageName : "",
                                            pluginLocalName ? pluginLocalName : "",
                                            name ? C(name.get()) : "",
                                            manufacturer ? C(manufacturer.get()) : "",
                                            version ? C(version.get()) : "",
                                            uid ? C(uid.get()) : "",
                                            library ? C(library.get()) : "",
                                            entrypoint ? C(entrypoint.get()) : "",
                                            category ? C(category.get()) : "",
                                            xmlfile);
                for (xmlNode* pluginChild = pluginElem->children; pluginChild; pluginChild = pluginChild->next) {
                    if (pluginChild->type != XML_ELEMENT_NODE ||
                        strcmp(C(pluginChild->name), "ports") ||
                        pluginChild->ns != nullptr)
                        continue;
                    auto portsElement = pluginChild;
                    for (xmlNode* portsChild = portsElement->children; portsChild; portsChild = portsChild->next) {
                        if (portsChild->type != XML_ELEMENT_NODE ||
                            strcmp(C(portsChild->name), "port") ||
                            portsChild->ns != nullptr)
                            continue;
                        auto portElement = portsChild;

                        auto portName = std::unique_ptr<const char>(C(xmlGetNsProp(portElement, XC("name"), nullptr)));
                        auto contentString = std::unique_ptr<const char>(C(xmlGetNsProp(portElement, XC("content"), nullptr)));
                        ContentType content =
                                !strcmp(contentString.get(), "audio") ? ContentType::AAP_CONTENT_TYPE_AUDIO :
                                !strcmp(contentString.get(), "midi") ? ContentType::AAP_CONTENT_TYPE_MIDI :
                                !strcmp(contentString.get(), "midi2") ? ContentType::AAP_CONTENT_TYPE_MIDI2 :
                                ContentType::AAP_CONTENT_TYPE_UNDEFINED;
                        auto directionString = std::unique_ptr<const char>(C(xmlGetNsProp(portElement, XC("direction"), nullptr)));
                        PortDirection  direction = !strcmp(directionString.get(), "input") ? PortDirection::AAP_PORT_DIRECTION_INPUT : PortDirection::AAP_PORT_DIRECTION_OUTPUT;
                        // FIXME: specify correct XML namespace URI
                        auto defaultValue = std::unique_ptr<const char>(C(xmlGetNsProp(portElement, XC("default"), nullptr)));
                        auto minimumValue = std::unique_ptr<const char>(C(xmlGetNsProp(portElement, XC("minimum"), nullptr)));
                        auto maximumValue = std::unique_ptr<const char>(C(xmlGetNsProp(portElement, XC("maximum"), nullptr)));
                        auto nativePort = new PortInformation(portName.get(), content, direction);
                        if (defaultValue != nullptr || minimumValue != nullptr || maximumValue != nullptr) {
                            nativePort->setPropertyValueString(AAP_PORT_DEFAULT, defaultValue.get());
                            nativePort->setPropertyValueString(AAP_PORT_MINIMUM, minimumValue.get());
                            nativePort->setPropertyValueString(AAP_PORT_MAXIMUM, maximumValue.get());
                        }
                        plugin->addPort(nativePort);
                    }
                }
                plugins.push_back(plugin);
            }
        }
    }
    xmlFreeDoc(xmlDoc);
    xmlFreeParserCtxt(xmlParserCtxt);
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
