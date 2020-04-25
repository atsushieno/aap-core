#pragma once

#ifndef _ANDROID_AUDIO_PLUGIN_HOST_HPP_
#define _ANDROID_AUDIO_PLUGIN_HOST_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <map>
#include <string>
#include "aap/android-audio-plugin.h"

namespace aap {

class PluginInstance;

enum ContentType {
	AAP_CONTENT_TYPE_UNDEFINED,
	AAP_CONTENT_TYPE_AUDIO,
	AAP_CONTENT_TYPE_MIDI,
};

enum PortDirection {
	AAP_PORT_DIRECTION_INPUT,
	AAP_PORT_DIRECTION_OUTPUT
};

enum PluginInstantiationState {
	PLUGIN_INSTANTIATION_STATE_UNPREPARED,
	PLUGIN_INSTANTIATION_STATE_INACTIVE,
	PLUGIN_INSTANTIATION_STATE_ACTIVE,
	PLUGIN_INSTANTIATION_STATE_TERMINATED,
};

class PortInformation
{
	std::string name{};
	ContentType content_type;
	PortDirection direction;
	bool has_value_range{false};
	float default_value{0.0f};
	float minimum_value{0.0f};
	float maximum_value{1.0f};
	
public:
	PortInformation(const char *portName, ContentType content, PortDirection portDirection)
			: name(portName), content_type(content), direction(portDirection)
	{
	}

	PortInformation(const char *portName, ContentType content, PortDirection portDirection,
			float defaultValue, float minimumValue, float maximumValue)
		: PortInformation(portName, content, portDirection)
	{
		has_value_range = true;
		default_value = defaultValue;
		minimum_value = minimumValue;
		maximum_value = maximumValue;
	}

	const char* getName() const { return name.c_str(); }
	ContentType getContentType() const { return content_type; }
	PortDirection getPortDirection() const { return direction; }
	bool hasValueRange() const { return has_value_range; }
	float getDefaultValue() const { return default_value; }
	float getMinimumValue() const { return minimum_value; }
	float getMaximumValue() const { return maximum_value; }
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
	long last_info_updated_unixtime;

	/* NULL-terminated list of categories */
	std::string primary_category{};
	/* NULL-terminated list of ports */
	std::vector<const PortInformation*> ports;
	/* NULL-terminated list of required extensions */
	std::vector<const AndroidAudioPluginExtension *> required_extensions;
	/* NULL-terminated list of optional extensions */
	std::vector<const AndroidAudioPluginExtension *> optional_extensions;

public:
	static std::vector<PluginInformation*> parsePluginMetadataXml(const char* pluginPackageName, const char* pluginLocalName, const char* xmlfile);

	/* In VST3 world, they are like "Effect", "Synth", "Instrument|Synth", "Fx|Delay" ... can be anything. Here we list typical-looking ones */
	const char * PRIMARY_CATEGORY_EFFECT = "Effect";
	const char * PRIMARY_CATEGORY_SYNTH = "Synth";
	
	PluginInformation(bool isOutProcess, std::string pluginPackageName, std::string pluginLocalName, std::string displayName, std::string manufacturerName, std::string versionString, std::string pluginID, std::string sharedLibraryFilename, std::string libraryEntrypoint)
		: is_out_process(isOutProcess),
		  plugin_package_name(pluginPackageName),
		  plugin_local_name(pluginLocalName),
		  display_name(displayName),
		  manufacturer_name(manufacturerName),
		  version(versionString),
		  shared_library_filename(sharedLibraryFilename),
		  library_entrypoint(libraryEntrypoint),
		  plugin_id(pluginID),
		  last_info_updated_unixtime((long) time(NULL))
	{
		char *cp;
		int len = snprintf(nullptr, 0, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
		cp = (char*) calloc(len, 1);
		snprintf(cp, len, "%s+%s+%s", display_name.c_str(), plugin_id.c_str(), version.c_str());
		identifier_string = cp;
        free(cp);
	}
	
	~PluginInformation()
	{
	}

	const std::string getPluginPackageName() const
	{
		return plugin_package_name;
	}

	const std::string getPluginLocalName() const
	{
		return plugin_local_name;
	}

	const std::string getDisplayName() const
	{
		return display_name;
	}
	
	const std::string getManufacturerName() const
	{
		return manufacturer_name;
	}
	
	const std::string getVersion() const
	{
		return version;
	}
	
	/* locally identifiable string.
	 * It is combination of name+unique_id+version, to support plugin debugging. */
	const std::string getStrictIdentifier() const
	{
		return identifier_string;
	}
	
	const std::string getPrimaryCategory() const
	{
		return primary_category;
	}
	
	int32_t getNumPorts() const
	{
		return ports.size();
	}
	
	const PortInformation* getPort(int32_t index) const
	{
		return ports[index];
	}
	
	int32_t getNumRequiredExtensions() const
	{
		return required_extensions.size();
	}
	
	const AndroidAudioPluginExtension *getRequiredExtension(int32_t index) const
	{
		return required_extensions[index];
	}
	
	int32_t getNumOptionalExtensions() const
	{
		return optional_extensions.size();
	}
	
	const AndroidAudioPluginExtension *getOptionalExtension(int32_t index) const
	{
		return optional_extensions[index];
	}
	
	long getLastInfoUpdateTime() const
	{
		return last_info_updated_unixtime;
	}
	
	/* unique identifier across various environment */
	const std::string getPluginID() const
	{
		return plugin_id;
	}
	
	bool isInstrument() const
	{
		// The purpose of this function seems to be based on hacky premise. So do we.
		return strstr(getPrimaryCategory().c_str(), "Instrument") != NULL;
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

	const std::string getLocalPluginSharedLibrary() const
	{
		// By metadata or inferred.
		// Since Android expects libraries stored in `lib` directory,
		// it will just return the name of the shared library.
		return shared_library_filename;
	}

	const std::string getLocalPluginLibraryEntryPoint() const
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

class EditorInstance
{
	//const PluginInstance *owner;

public:

	EditorInstance(const PluginInstance *ownerPlugin)
	//	: owner(ownerPlugin)
	{
		// TODO: FUTURE (v0.4)
	}

	void startEditorUI()
	{
		// TODO: FUTURE (v0.4)
	}
};


class PluginHostPAL
{
protected:
	// FIXME: move to PluginHost
	std::vector<PluginInformation*> plugin_list_cache{};

public:
	~PluginHostPAL() {
		for (auto plugin : plugin_list_cache)
			delete plugin;
	}

	virtual std::vector<std::string> getPluginPaths() = 0;
	virtual void getAAPMetadataPaths(std::string path, std::vector<std::string>& results) = 0;
	virtual std::vector<PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) = 0;

	// FIXME: move to PluginHost
	std::vector<PluginInformation*> getPluginListCache() { return plugin_list_cache; }

	// FIXME: move to PluginHost
    void setPluginListCache(std::vector<PluginInformation*> pluginInfos) {
    	plugin_list_cache.clear();
    	for (auto p : pluginInfos)
    		plugin_list_cache.emplace_back(p);
    }

	std::vector<PluginInformation*> getInstalledPlugins(bool returnCacheIfExists = true, std::vector<std::string>* searchPaths = nullptr) {
		auto ret = plugin_list_cache;
		if (ret.size() > 0)
			return ret;
		std::vector<std::string> aapPaths{};
		for (auto path : getPluginPaths())
			getAAPMetadataPaths(path, aapPaths);
		ret = getPluginsFromMetadataPaths(aapPaths);
		return ret;
	}
};

PluginHostPAL* getPluginHostPAL();


class SharedMemoryExtension
{
    std::vector<int64_t> port_buffer_fds{};
	std::vector<int64_t> extension_fds{};

public:
    static char const *URI;

    SharedMemoryExtension() {}
    ~SharedMemoryExtension() {
        for (int64_t fd : port_buffer_fds)
            close(fd);
        port_buffer_fds.clear();
		for (int64_t fd : extension_fds)
			close(fd);
		extension_fds.clear();
    }

    std::vector<int64_t>& getPortBufferFDs() { return port_buffer_fds; }
	std::vector<int64_t>& getExtensionFDs() { return extension_fds; }
};

class PluginHostManager;

class PluginHost
{
	std::vector<PluginInstance*> instances{};
	PluginHostManager* manager{nullptr};

	PluginInstance* instantiateLocalPlugin(const PluginInformation *pluginInfo, int sampleRate);
	PluginInstance* instantiateRemotePlugin(const PluginInformation *pluginInfo, int sampleRate);

public:
	PluginHost(PluginHostManager* manager)
		: manager(manager)
	{
	}

	int createInstance(const char* identifier, int sampleRate);
	void destroyInstance(PluginInstance* instance);
	int32_t getInstanceCount() { return instances.size(); }
	PluginInstance* getInstance(int32_t index) { return instances[index]; }
};

class PluginHostManager
{
	std::vector<const PluginInformation*> plugin_infos{};

public:

	PluginHostManager();

	~PluginHostManager()
	{
	}

	void updateKnownPlugins(std::vector<PluginInformation*> plugins);
	
	bool isPluginAlive (const char *identifier);
	
	bool isPluginUpToDate (const char *identifier, long lastInfoUpdated);

	int32_t getNumPluginInformation()
	{
		return plugin_infos.size();
	}

	const PluginInformation* getPluginInformation(int index)
	{
		return plugin_infos[index];
	}
	
	const PluginInformation* getPluginInformation(const char *identifier)
	{
		int n = getNumPluginInformation();
		for(int i = 0; i < n; i++) {
			auto d = getPluginInformation(i);
			if (d->getPluginID().compare(identifier) == 0)
				return d;
		}
		return NULL;
	}
};

class PluginInstance
{
	friend class PluginHost;
	friend class PluginHostManager;
	
	PluginHost *host;
	int sample_rate{44100};
	const PluginInformation *pluginInfo;
	AndroidAudioPluginFactory *plugin_factory;
	AndroidAudioPlugin *plugin;
	std::vector<AndroidAudioPluginExtension> extensions{};
	PluginInstantiationState instantiation_state;
	AndroidAudioPluginState plugin_state{0, nullptr};

	PluginInstance(PluginHost* pluginHost, const PluginInformation* pluginInfo, AndroidAudioPluginFactory* loadedPluginFactory, int sampleRate)
		: host(pluginHost),
		  sample_rate(sampleRate),
		  pluginInfo(pluginInfo),
		  plugin_factory(loadedPluginFactory),
		  plugin(nullptr),
		  instantiation_state(PLUGIN_INSTANTIATION_STATE_UNPREPARED)
	{
	}
	
public:

    ~PluginInstance() { dispose(); }

	const PluginInformation* getPluginInformation()
	{
		return pluginInfo;
	}

	void addExtension(AndroidAudioPluginExtension ext)
	{
		extensions.emplace_back(ext);
	}

	const void* getExtension(const char* uri)
	{
		for (auto ext : extensions)
			if (strcmp(ext.uri, uri) == 0)
				return ext.data;
		return nullptr;
	}

	inline SharedMemoryExtension* getSharedMemoryExtension() { return (SharedMemoryExtension*) getExtension(SharedMemoryExtension::URI); }

	void prepare(int maximumExpectedSamplesPerBlock, AndroidAudioPluginBuffer *preparedBuffer)
	{
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED || instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);

		auto extArr = (AndroidAudioPluginExtension**) calloc(sizeof(AndroidAudioPluginExtension*), extensions.size());
		for (int i = 0; i < extensions.size(); i++)
			extArr[i] = &extensions[i];
		plugin = plugin_factory->instantiate(plugin_factory, pluginInfo->getPluginID().c_str(), sample_rate, extArr);
		free(extArr);
		plugin->prepare(plugin, preparedBuffer);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void activate()
	{
		if (instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
			return;
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);
		
		plugin->activate(plugin);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
	}
	
	void deactivate()
	{
		if (instantiation_state == PLUGIN_INSTANTIATION_STATE_INACTIVE)
			return;
		assert(instantiation_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);
		
		plugin->deactivate(plugin);
		instantiation_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void dispose()
	{
		if (plugin != nullptr)
			plugin_factory->release(plugin_factory, plugin);
		plugin = nullptr;
		instantiation_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
	}
	
	void process(AndroidAudioPluginBuffer *buffer, int32_t timeoutInNanoseconds)
	{
		// It is not a TODO here, but if pointers have changed, we have to reconnect
		// LV2 ports.
		plugin->process(plugin, buffer, timeoutInNanoseconds);
	}

	EditorInstance* createEditor()
	{
		// FIXME: implement
		return nullptr;//new EditorInstance(instance);
	}
	
	int getNumPrograms()
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
		return 0;
	}
	
	int getCurrentProgram()
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
		return 0;
	}
	
	void setCurrentProgram(int index)
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it, but resets all parameters.
	}
	
	const char * getProgramName(int index)
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
		return NULL;
	}
	
	void changeProgramName(int index, const char * newName)
	{
		// TODO: FUTURE (v0.6). LADSPA does not support it either.
	}

	int32_t getStateSize()
	{
		AndroidAudioPluginState result{0, nullptr};
		plugin->get_state(plugin, &result);
		return result.data_size;
	}

	const AndroidAudioPluginState& getState()
	{
		AndroidAudioPluginState result{0, nullptr};
		plugin->get_state(plugin, &result);
		if (plugin_state.data_size < result.data_size) {
			if (plugin_state.raw_data != nullptr)
				free((void*) plugin_state.raw_data);
			plugin_state.raw_data = calloc(1, result.data_size);
		}
		plugin_state.data_size = result.data_size;
		plugin->get_state(plugin, &plugin_state);

		return plugin_state;
	}
	
	void setState(const void* data, int32_t sizeInBytes)
	{
		AndroidAudioPluginState state;
		state.data_size = sizeInBytes;
		state.raw_data = data;
		plugin->set_state(plugin, &state);
	}
	
	uint32_t getTailTimeInMilliseconds()
	{
		// TODO: FUTURE (v0.6) - most likely just a matter of plugin property
		return 0;
	}
};

} // namespace

#endif // _ANDROID_AUDIO_PLUGIN_HOST_HPP_

