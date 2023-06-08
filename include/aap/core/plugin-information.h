#ifndef AAP_CORE_PLUGIN_INFORMATION_H
#define AAP_CORE_PLUGIN_INFORMATION_H

#include <string>
#include <vector>
#include <map>
#include "aap/ext/plugin-info.h"

#define AAP_PORT_URL "urn:org.androidaudioplugin.port"
#define AAP_PORT_BASE AAP_PORT_URL "#"
#define AAP_PORT_MINIMUM_SIZE AAP_PORT_BASE "minimumSize"

namespace aap {


enum PluginInstantiationState {
    PLUGIN_INSTANTIATION_STATE_INITIAL,
    PLUGIN_INSTANTIATION_STATE_UNPREPARED,
    PLUGIN_INSTANTIATION_STATE_INACTIVE,
    PLUGIN_INSTANTIATION_STATE_ACTIVE,
    PLUGIN_INSTANTIATION_STATE_TERMINATED,
    PLUGIN_INSTANTIATION_STATE_ERROR
};

class PropertyContainer {
    std::map<std::string, std::string> properties{};

public:
    void setPropertyValueString(std::string propertyId, std::string value) {
        properties[propertyId] = value;
    }

    bool getPropertyAsBoolean(std::string propertyId) const {
        return getPropertyAsDouble(propertyId) > 0;
    }
    int getPropertyAsInteger(std::string propertyId) const {
        return hasProperty(propertyId) ? atoi(getPropertyAsString(propertyId).c_str()) : 0;
    }
    float getPropertyAsFloat(std::string propertyId) const {
        return hasProperty(propertyId) ? (float) atof(getPropertyAsString(propertyId).c_str()) : 0.0f;
    }
    double getPropertyAsDouble(std::string propertyId) const {
        return hasProperty(propertyId) ? atof(getPropertyAsString(propertyId).c_str()) : 0.0;
    }
    bool hasProperty(std::string propertyId) const {
        return properties.find(propertyId) != properties.end();
    }
    std::string getPropertyAsString(std::string propertyId) const {
        return hasProperty(propertyId) ? properties.find(propertyId)->second : "";
    }
};

class PortInformation : public PropertyContainer {
    uint32_t index{0};
    std::string name{};
    aap_content_type content_type;
    aap_port_direction direction;

public:
    PortInformation(uint32_t portIndex, std::string portName, aap_content_type content, aap_port_direction portDirection)
            : index(portIndex), name(portName), content_type(content), direction(portDirection)
    {
    }

    int32_t getIndex() const { return index; }
    const char* getName() const { return name.c_str(); }
    aap_content_type getContentType() const { return content_type; }
    aap_port_direction getPortDirection() const { return direction; }
};

class ParameterInformation : public PropertyContainer {
    int32_t id{0};
    std::string name{};
    double min_value;
    double max_value;
    double default_value;
    double priority;

public:
    ParameterInformation(int32_t id, std::string name, double minValue, double maxValue, double defaultValue, double priority = 0)
            : id(id), name(name), min_value(minValue), max_value(maxValue), default_value(defaultValue), priority(priority)
    {
    }

    int32_t getId() const { return id; }
    const char* getName() const { return name.c_str(); }
    double getMinimumValue() const { return min_value; }
    double getMaximumValue() const { return max_value; }
    double getDefaultValue() const { return default_value; }
    double getPriority() const { return priority; }
};

class PluginExtensionInformation
{
public:
    bool required{false};
    std::string uri{};
};

class PluginInformation
{
    // hosting information
    bool is_out_process;

    int64_t last_info_updated_unixtime_milliseconds;

    // basic information
    std::string plugin_package_name{}; // service package name for Android, FIXME: determine semantics for desktop
    std::string plugin_local_name{}; // service class name for Android, FIXME: determine semantics for desktop
    std::string display_name{};
    std::string developer_name{};
    std::string version{};
    std::string identifier_string{};
    std::string shared_library_filename{};
    std::string library_entrypoint{};
    std::string plugin_id{};
    std::string metadata_full_path{};

    /** NULL-terminated list of categories, separate by | */
    std::string primary_category{};
    /** UI View factory class name */
    std::string ui_view_factory{};
    /** UI Activity class name */
    std::string ui_activity{};
    /** UI Web archive name */
    std::string ui_web{};
    /** list of ports */
    std::vector<const PortInformation*> declared_ports;
    /** list of parameters */
    std::vector<const ParameterInformation*> parameters;
    /** list of extensions. They may be either required or optional */
    std::vector<PluginExtensionInformation> extensions;

public:

    /* In VST3 world, they are like "Effect", "Instrument", "Instrument|Synth", "Fx|Delay" ... can be anything. Here we list typical-looking ones */
    const char * PRIMARY_CATEGORY_EFFECT = "Effect";
    const char * PRIMARY_CATEGORY_INSTRUMENT = "Instrument";

    PluginInformation(bool isOutProcess, const char* pluginPackageName,
                      const char* pluginLocalName, const char* displayName,
                      const char* developerName, const char* versionString,
                      const char* pluginID, const char* sharedLibraryFilename,
                      const char* libraryEntrypoint, const char* metadataFullPath,
                      const char* primaryCategory, const char* uiViewFactory,
                      const char* uiActivity, const char* uiWeb);

    ~PluginInformation()
    {
    }

    const std::string& getPluginPackageName() const
    {
        return plugin_package_name;
    }

    const std::string& getPluginLocalName() const
    {
        return plugin_local_name;
    }

    const std::string& getDisplayName() const
    {
        return display_name;
    }

    const std::string& getDeveloperName() const
    {
        return developer_name;
    }

    const std::string& getVersion() const
    {
        return version;
    }

    /* locally identifiable string.
     * It is combination of name+unique_id+version, to support plugin debugging. */
    const std::string& getStrictIdentifier() const
    {
        return identifier_string;
    }

    const std::string& getPrimaryCategory() const
    {
        return primary_category;
    }

    const std::string& getUiWeb() const
    {
        return ui_web;
    }

    const std::string& getUiViewFactory() const
    {
        return ui_view_factory;
    }

    const std::string& getUiActivity() const
    {
        return ui_activity;
    }

    int getNumDeclaredPorts() const
    {
        return (int) declared_ports.size();
    }

    const PortInformation* getDeclaredPort(int index) const
    {
        return declared_ports[(size_t) index];
    }

    void addDeclaredPort(PortInformation* port)
    {
        declared_ports.emplace_back(port);
    }

    int getNumDeclaredParameters() const
    {
        return (int) parameters.size();
    }

    const ParameterInformation* getDeclaredParameter(int index) const
    {
        return parameters[(size_t) index];
    }

    void addDeclaredParameter(ParameterInformation* parameter)
    {
        parameters.emplace_back(parameter);
    }

    int getNumExtensions() const
    {
        return (int) extensions.size();
    }

    PluginExtensionInformation getExtension(int index) const
    {
        return extensions[(size_t) index];
    }

    PluginExtensionInformation getExtension(const char* uri) const;

    bool hasExtension(const char *uri) const {
        return uri != nullptr && strlen(uri) > 0 && !getExtension(uri).uri.empty();
    }

    int64_t getLastInfoUpdateTime() const
    {
        return last_info_updated_unixtime_milliseconds;
    }

    /* unique identifier across various environment */
    const std::string& getPluginID() const
    {
        return plugin_id;
    }

    const std::string& getMetadataFullPath() const
    {
        return metadata_full_path;
    }

    bool isInstrument() const
    {
        // The purpose of this function seems to be based on hacky premise. So do we.
        return strstr(getPrimaryCategory().c_str(), "Instrument") != nullptr;
    }

    bool hasEditor() const
    {
        // TODO: FUTURE
        return false;
    }

    const std::string& getLocalPluginSharedLibrary() const
    {
        // By metadata or inferred.
        // Since Android expects libraries stored in `lib` directory,
        // it will just return the name of the shared library.
        return shared_library_filename;
    }

    const std::string& getLocalPluginLibraryEntryPoint() const
    {
        // can be null. "GetAndroidAudioPluginFactory" is used by default.
        return library_entrypoint;
    }

    bool isOutProcess() const
    {
        return is_out_process;
    }

    void addExtension(PluginExtensionInformation extension)
    {
        extensions.emplace_back(extension);
    }
};


class PluginServiceInformation {
    std::string package_name;
    std::string class_name;

public:
    PluginServiceInformation(std::string &packageName, std::string &className)
            : package_name(packageName), class_name(className) {
    }

    inline std::string & getPackageName() { return package_name; }
    inline std::string & getClassName() { return class_name; }
};


} // namespace aap

#endif //AAP_CORE_PLUGIN_INFORMATION_H
