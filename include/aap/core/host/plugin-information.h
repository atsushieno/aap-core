#ifndef AAP_CORE_PLUGIN_INFORMATION_H
#define AAP_CORE_PLUGIN_INFORMATION_H

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
};

class PortInformation
{
    uint32_t index{0};
    std::string name{};
    ContentType content_type;
    PortDirection direction;
    std::map<std::string, std::string> properties{};

public:
    PortInformation(uint32_t portIndex, std::string portName, ContentType content, PortDirection portDirection)
            : index(portIndex), name(portName), content_type(content), direction(portDirection)
    {
    }

    uint32_t getIndex() const { return index; }
    const char* getName() const { return name.c_str(); }
    ContentType getContentType() const { return content_type; }
    PortDirection getPortDirection() const { return direction; }

    // deprecated
    bool hasValueRange() const { return hasProperty(AAP_PORT_DEFAULT); }

    void setPropertyValueString(std::string id, std::string value) {
        properties[id] = value;
    }

    // deprecated
    float getDefaultValue() const { return hasProperty(AAP_PORT_DEFAULT) ? getPropertyAsFloat(AAP_PORT_DEFAULT) : 0.0f; }
    // deprecated
    float getMinimumValue() const { return hasProperty(AAP_PORT_MINIMUM) ? getPropertyAsFloat(AAP_PORT_MINIMUM) : 0.0f; }
    // deprecated
    float getMaximumValue() const { return hasProperty(AAP_PORT_MAXIMUM) ? getPropertyAsFloat(AAP_PORT_MAXIMUM) : 0.0f; }

    bool getPropertyAsBoolean(std::string id) const {
        return getPropertyAsDouble(id) > 0;
    }
    int getPropertyAsInteger(std::string id) const {
        return atoi(getPropertyAsString(id).c_str());
    }
    float getPropertyAsFloat(std::string id) const {
        return (float) atof(getPropertyAsString(id).c_str());
    }
    double getPropertyAsDouble(std::string id) const {
        return atof(getPropertyAsString(id).c_str());
    }
    bool hasProperty(std::string id) const {
        return properties.find(id) != properties.end();
    }
    std::string getPropertyAsString(std::string id) const {
        return properties.find(id)->second;
    }
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

    /* NULL-terminated list of categories, separate by | */
    std::string primary_category{};
    /* NULL-terminated list of ports */
    std::vector<const PortInformation*> ports;
    /* NULL-terminated list of required extensions */
    std::vector<std::unique_ptr<std::string>> required_extensions;
    /* NULL-terminated list of optional extensions */
    std::vector<std::unique_ptr<std::string>> optional_extensions;

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

    int getNumPorts() const
    {
        return (int) ports.size();
    }

    const PortInformation* getPort(int index) const
    {
        return ports[(size_t) index];
    }

    int getNumRequiredExtensions() const
    {
        return (int) required_extensions.size();
    }

    const std::string* getRequiredExtension(int index) const
    {
        return required_extensions[(size_t) index].get();
    }

    int getNumOptionalExtensions() const
    {
        return (int) optional_extensions.size();
    }

    const std::string* getOptionalExtension(int index) const
    {
        return optional_extensions[(size_t) index].get();
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

    bool hasMidi2Ports() const
    {
        for (auto port : ports)
            if (port->getContentType() == AAP_CONTENT_TYPE_MIDI2)
                return true;
        return false;
    }

    bool hasSharedContainer() const
    {
        // TODO: FUTURE (v0.8) It may be something AAP should support because
        // context switching over outprocess plugins can be quite annoying...
        return false;
    }

    bool hasEditor() const
    {
        // TODO: FUTURE (v0.7)
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

    void addPort(PortInformation* port)
    {
        ports.push_back(port);
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


class PluginListSnapshot {
    std::vector<const PluginServiceInformation*> services{};
    std::vector<const PluginInformation*> plugins{};

public:
    static PluginListSnapshot queryServices();

    size_t getNumPluginInformation()
    {
        return plugins.size();
    }

    const PluginInformation* getPluginInformation(int32_t index)
    {
        return plugins[(size_t) index];
    }

    const PluginInformation* getPluginInformation(std::string identifier)
    {
        for (auto plugin : plugins) {
            if (plugin->getPluginID().compare(identifier) == 0)
                return plugin;
        }
        return nullptr;
    }
};


class PluginClientConnection {
    std::string package_name;
    std::string class_name;
    void * connection_data;

public:
    PluginClientConnection(std::string packageName, std::string className, void * connectionData)
            : package_name(packageName), class_name(className), connection_data(connectionData) {}

    inline std::string & getPackageName() { return package_name; }
    inline std::string & getClassName() { return class_name; }
    inline void * getConnectionData() { return connection_data; }
};

class PluginClientConnectionList {
    std::vector<PluginClientConnection*> serviceConnections{};

public:
    inline void add(std::unique_ptr<PluginClientConnection> entry) {
        serviceConnections.emplace_back(entry.release());
    }

    inline void remove(std::string packageName, std::string className) {
        for (size_t i = 0; i < serviceConnections.size(); i++) {
            auto &c = serviceConnections[i];
            if (c->getPackageName() == packageName && c->getClassName() == className) {
                delete serviceConnections[i];
                serviceConnections.erase(serviceConnections.begin() + i);
                break;
            }
        }
    }

    void* getServiceHandleForConnectedPlugin(std::string packageName, std::string className);

    void* getServiceHandleForConnectedPlugin(std::string pluginId);
};


} // namespace aap

#endif //AAP_CORE_PLUGIN_INFORMATION_H
