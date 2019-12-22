#pragma once

#ifndef _ANDROID_AUDIO_PLUGIN_HOST_HPP_
#define _ANDROID_AUDIO_PLUGIN_HOST_HPP_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <time.h>
#include <vector>
#include <string>
#include "aap/android-audio-plugin.h"


namespace aap
{

class PluginInstance;
class AAPEditor;

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
	
public:
	PortInformation(const char *portName, ContentType content, PortDirection portDirection)
		: name(portName), content_type(content), direction(portDirection)
	{
	}

	const char* getName() const { return name.data(); }
	ContentType getContentType() const { return content_type; }
	PortDirection getPortDirection() const { return direction; }
};

#define safe_strdup(s) ((const char*) s ? strdup(s) : NULL)

#define SAFE_FREE(s) if (s) { free((void*) s); s = NULL; }

class PluginInformation
{
	// hosting information
	bool is_out_process;

	// basic information
	std::string name{};
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
	static PluginInformation** ParsePluginDescriptor(const char* xmlfile);

	/* In VST3 world, they are like "Effect", "Synth", "Instrument|Synth", "Fx|Delay" ... can be anything. Here we list typical-looking ones */
	const char * PRIMARY_CATEGORY_EFFECT = "Effect";
	const char * PRIMARY_CATEGORY_SYNTH = "Synth";
	
	PluginInformation(bool isOutProcess, const char* pluginName, const char* manufacturerName, const char* versionString, const char* pluginID, const char* sharedLibraryFilename, const char *libraryEntrypoint)
		: is_out_process(isOutProcess),
		  name(pluginName),
		  manufacturer_name(manufacturerName ? manufacturerName : ""),
		  version(versionString ? versionString : ""),
		  shared_library_filename(sharedLibraryFilename ? sharedLibraryFilename : ""),
		  library_entrypoint(libraryEntrypoint ? libraryEntrypoint : ""),
		  plugin_id(pluginID ? pluginID : ""),
		  last_info_updated_unixtime((long) time(NULL))
	{
		char *cp;
		int len = snprintf(nullptr, 0, "%s+%s+%s", name.data(), plugin_id.data(), version.data());
		cp = (char*) calloc(len, 1);
		snprintf(cp, len, "%s+%s+%s", name.data(), plugin_id.data(), version.data());
		identifier_string = cp;
	}
	
	~PluginInformation()
	{
	}
	
	const std::string getName() const
	{
		return name;
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
	const std::string getIdentifier() const
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
	
	const PortInformation *getPort(int32_t index) const
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
		return strstr(getPrimaryCategory().data(), "Instrument") != NULL;
	}
	
	bool hasSharedContainer() const
	{
		// TODO: FUTURE (v0.6) It may be something AAP should support because
		// context switching over outprocess plugins can be quite annoying...
		return false;
	}
	
	bool hasEditor() const
	{
		// TODO: FUTURE (v0.4)
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

PluginInformation** aap_parse_plugin_descriptor(const char* xmlfile);

class EditorInstance
{
	const PluginInstance *owner;

public:

	EditorInstance(const PluginInstance *ownerPlugin)
		: owner(ownerPlugin)
	{
		// TODO: FUTURE (v0.4)
	}

	void startEditorUI()
	{
		// TODO: FUTURE (v0.4)
	}
};

class PluginHostBackend
{
};

class PluginHostBackendLV2 : public PluginHostBackend
{
};

class PluginHostBackendVST3 : public PluginHostBackend
{
};

class PluginHost
{
	PluginHostBackendLV2 backend_lv2;
	PluginHostBackendLV2 backend_vst3;

	std::vector<const PluginHostBackend*> backends;
	
	std::vector<const PluginInformation*> plugin_descriptors;
	
	PluginInstance* instantiateLocalPlugin(const PluginInformation *descriptor);
	PluginInstance* instantiateRemotePlugin(const PluginInformation *descriptor);

public:

	PluginHost(const PluginInformation* const* pluginDescriptors);

	~PluginHost()
	{
	}
	
	bool isPluginAlive (const char *identifier);
	
	bool isPluginUpToDate (const char *identifier, long lastInfoUpdated);
	
	void addHostBackend(const PluginHostBackend *backend)
	{
		assert(backend != NULL);
		backends.push_back(backend);
	}
	
	int32_t getNumHostBackends()
	{
		return backends.size();
	}

	const PluginHostBackend* getHostBackend(int index)
	{
		return backends[index];
	}
	
	int32_t getNumPluginDescriptors()
	{
		return plugin_descriptors.size();
	}

	const PluginInformation* getPluginDescriptorAt(int index)
	{
		return plugin_descriptors[index];
	}
	
	const PluginInformation* getPluginDescriptor(const char *identifier)
	{
		int n = getNumPluginDescriptors();
		for(int i = 0; i < n; i++) {
			auto d = getPluginDescriptorAt(i);
			auto id = d->getPluginID().data();
			if (strcmp(id, identifier) == 0)
				return d;
		}
		return NULL;
	}
	
	PluginInstance* instantiatePlugin(const char* identifier);
	
	EditorInstance* createEditor(PluginInstance* instance)
	{
		return new EditorInstance(instance);
	}
};

class PluginInstance
{
	friend class PluginHost;
	
	PluginHost *host;
	const PluginInformation *descriptor;
	AndroidAudioPluginFactory *plugin_factory;
	AndroidAudioPlugin *plugin;
	const AndroidAudioPluginExtension * const *extensions;
	PluginInstantiationState plugin_state;

	PluginInstance(PluginHost* pluginHost, const PluginInformation* pluginDescriptor, AndroidAudioPluginFactory* loadedPluginFactory)
		: host(pluginHost),
		  descriptor(pluginDescriptor),
		  plugin_factory(loadedPluginFactory),
		  plugin(NULL),
		  extensions(NULL),
		  plugin_state(PLUGIN_INSTANTIATION_STATE_UNPREPARED)
	{
	}
	
public:

    ~PluginInstance() { dispose(); }

	const PluginInformation* getPluginDescriptor()
	{
		return descriptor;
	}

	void prepare(double sampleRate, int maximumExpectedSamplesPerBlock, AndroidAudioPluginBuffer *preparedBuffer)
	{
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED);
		
		plugin = plugin_factory->instantiate(plugin_factory, descriptor->getPluginID().data(), sampleRate, extensions);
		plugin->prepare(plugin, preparedBuffer);
		plugin_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void activate()
	{
		if (plugin_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
			return;
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);
		
		plugin->activate(plugin);
		plugin_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
	}
	
	void deactivate()
	{
		if (plugin_state == PLUGIN_INSTANTIATION_STATE_INACTIVE)
			return;
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);
		
		plugin->deactivate(plugin);
		plugin_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void dispose()
	{
		if (plugin != NULL)
			plugin_factory->release(plugin_factory, plugin);
		plugin = NULL;
		plugin_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
	}
	
	void process(AndroidAudioPluginBuffer *buffer, int32_t timeoutInNanoseconds)
	{
		// It is not a TODO here, but if pointers have changed, we have to reconnect
		// LV2 ports.
		plugin->process(plugin, buffer, timeoutInNanoseconds);
	}
	
	EditorInstance* createEditor()
	{
		return host->createEditor(this);
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
		return plugin->get_state(plugin)->data_size;
	}
	
	void const* getState()
	{
		return plugin->get_state(plugin)->raw_data;
	}
	
	void setState(const void* data, int32_t offset, int32_t sizeInBytes)
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

