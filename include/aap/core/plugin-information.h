#ifndef AAP_CORE_PLUGIN_INFORMATION_H
#define AAP_CORE_PLUGIN_INFORMATION_H

#include <string>
#include <vector>
#include <map>
#include "aap/port-properties.h"

namespace aap {


enum ContentType {
    AAP_CONTENT_TYPE_UNDEFINED = 0,
    AAP_CONTENT_TYPE_AUDIO = 1,
    AAP_CONTENT_TYPE_MIDI = 2,
    // FIXME: we will remove it. There will be only one MIDI port that would switch
    //  between MIDI2 and MIDI1, using MIDI-CI Set New Protocol.
    AAP_CONTENT_TYPE_MIDI2 = 3,
};

enum PortDirection {
    AAP_PORT_DIRECTION_INPUT,
    AAP_PORT_DIRECTION_OUTPUT
};

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
    ContentType content_type;
    PortDirection direction;

public:
    PortInformation(uint32_t portIndex, std::string portName, ContentType content, PortDirection portDirection)
            : index(portIndex), name(portName), content_type(content), direction(portDirection)
    {
    }

    int32_t getIndex() const { return index; }
    const char* getName() const { return name.c_str(); }
    ContentType getContentType() const { return content_type; }
    PortDirection getPortDirection() const { return direction; }
};

class ParameterInformation : public PropertyContainer {
    int32_t id{0};
    std::string name{};
    double min_value;
    double max_value;
    double default_value;

public:
    ParameterInformation(int32_t id, std::string name, double minValue, double maxValue, double defaultValue)
            : id(id), name(name), min_value(minValue), max_value(maxValue), default_value(defaultValue)
    {
    }

    int32_t getId() const { return id; }
    const char* getName() const { return name.c_str(); }
    double getMinimumValue() const { return min_value; }
    double getMaximumValue() const { return max_value; }
    double getDefaultValue() const { return default_value; }
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

    // basic information
    std::string plugin_package_name{}; // service package name for Android, FIXME: determine semantics for desktop
    std::string plugin_local_name{}; // service class name for Android, FIXME: determine semantics for desktop
    std::string display_name{};
    std::string manufacturer_name{};
    std::string version{};
    std::string identifier_string{};
    std::string shared_library_filename{};
    std::string library_entrypoint{};
    std::string plugin_id{};
    std::string metadata_full_path{};
    int64_t last_info_updated_unixtime_milliseconds;

    /** NULL-terminated list of categories, separate by | */
    std::string primary_category{};
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
                      const char* manufacturerName, const char* versionString,
                      const char* pluginID, const char* sharedLibraryFilename,
                      const char* libraryEntrypoint, const char* metadataFullPath,
                      const char* primaryCategory);

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

    const std::string& getManufacturerName() const
    {
        return manufacturer_name;
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

    /* They are deleted. But if it we couldn't fix other parts (aap-lv2, aap-juce) it will be back.
    bool hasMidi2Ports() const
    {
        for (auto port : ports)
            if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2)
                return true;
        return false;
    }

    bool hasSharedContainer() const
    {
        // TODO: FUTURE It may be something AAP should support because
        // context switching over outprocess plugins can be quite annoying...
        return false;
    }
    */

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
