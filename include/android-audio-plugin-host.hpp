#pragma once

#include <assert.h>
#include <dlfcn.h>
#include <time.h>
#include "android/asset_manager.h"
#include "android-audio-plugin.h"


namespace aap
{

class AAPInstance;

enum AAPBufferType {
	AAP_BUFFER_TYPE_AUDIO,
	AAP_BUFFER_TYPE_CONTROL
};

enum AAPPortDirection {
	AAP_PORT_DIRECTION_INPUT,
	AAP_PORT_DIRECTION_OUTPUT
};

enum PluginInstantiationState {
	PLUGIN_INSTANTIATION_STATE_UNPREPARED,
	PLUGIN_INSTANTIATION_STATE_INACTIVE,
	PLUGIN_INSTANTIATION_STATE_ACTIVE,
	PLUGIN_INSTANTIATION_STATE_TERMINATED,
};

class AAPPortDescriptor
{
	const char *name;
	const AAPBufferType buffer_type;
	const AAPPortDirection direction;
	
public:
	const char* getName() { return name; }
	const AAPBufferType getBufferType() { return buffer_type; }
	const AAPPortDirection getPortDirection() { return direction; }
};

class AAPDescriptor
{
protected:
	const char *name;
	const char *display_name;
	const char *manufacturer_name;
	const char *version;
	const char *identifier_string;
	int unique_id;
	long last_info_updated_unixtime;

	/* NULL-terminated list of categories */
	const char *primary_category;
	/* NULL-terminated list of ports */
	AAPPortDescriptor **ports;
	/* NULL-terminated list of required extensions */
	AndroidAudioPluginExtension **required_extensions;
	/* NULL-terminated list of optional extensions */
	AndroidAudioPluginExtension **optional_extensions;

	// hosting information
	bool is_out_process;

public:
	const char * PRIMARY_CATEGORY_EFFECT = "Effect";
	const char * PRIMARY_CATEGORY_SYNTH = "Synth";
	
	AAPDescriptor(bool isOutProcess)
		: last_info_updated_unixtime((long) time(NULL)),
		  is_out_process(isOutProcess)
	{
	}
	
	const char* getName()
	{
		return name;
	}
	
	const char* getDescriptiveName()
	{
		return display_name;
	}
	
	const char* getManufacturerName()
	{
		return manufacturer_name;
	}
	
	const char* getVersion()
	{
		return version;
	}
	
	const char* getIdentifier()
	{
		return identifier_string;
	}
	
	const char* getPrimaryCategory()
	{
		return primary_category;
	}
	
	int32_t getNumPorts()
	{
		int32_t n = 0;
		auto ptr = ports;
		for (;ptr; ptr++)
			n++;
		return n;
	}
	
	AAPPortDescriptor *getPort(int32_t index)
	{
		assert(index < getNumPorts());
		return ports[index];
	}
	
	int32_t getNumRequiredExtensions()
	{
		int32_t n = 0;
		auto ptr = required_extensions;
		for (;ptr; ptr++)
			n++;
		return n;
	}
	
	AndroidAudioPluginExtension *getRequiredExtension(int32_t index)
	{
		assert(index < getNumRequiredExtensions());
		return required_extensions[index];
	}
	
	int32_t getNumOptionalExtensions()
	{
		int32_t n = 0;
		auto ptr = optional_extensions;
		for (;ptr; ptr++)
			n++;
		return n;
	}
	
	AndroidAudioPluginExtension *getOptionalExtension(int32_t index)
	{
		assert(index < getNumOptionalExtensions());
		return optional_extensions[index];
	}
	
	long getFileModTime()
	{
		// TODO: implement
		return 0;
	}
	
	long getLastInfoUpdateTime()
	{
		return last_info_updated_unixtime;
	}
	
	int32_t getUid()
	{
		return unique_id;
	}
	
	bool isInstrument()
	{
		// TODO: implement
		return false;
	}
	
	bool hasSharedContainer()
	{
		// TODO: implement
		return false;
	}
	
	bool hasEditor()
	{
		// TODO: implement
		return false;
	}
	
	bool hasMidiInputPort()
	{
		// TODO: implement
		return false;
	}
	
	bool hasMidiOutputPort()
	{
		// TODO: implement
		return false;
	}

	const char* getFilePath()
	{
		// TODO: implement
		return NULL;
	}
	
	bool isOutProcess()
	{
		return is_out_process;
	}
};

class AAPHost
{
	AAssetManager *asset_manager;
	const char* default_plugin_search_paths[2];
	AAPDescriptor** plugin_descriptors;
	
	AAPDescriptor* loadDescriptorFromBundleDirectory(const char *directory);
	AAPInstance* instantiateLocalPlugin(AAPDescriptor *descriptor);
	AAPInstance* instantiateRemotePlugin(AAPDescriptor *descriptor);

public:

	AAPHost()
		: asset_manager(NULL),
		  plugin_descriptors(NULL)
	{
		default_plugin_search_paths[0] = "/";
		default_plugin_search_paths[1] = NULL;
	}
	
	void initialize(AAssetManager *assetManager, const char* const *pluginIdentifiers);
	
	bool isPluginAlive (const char *identifier);
	
	bool isPluginUpToDate (const char *identifier, long lastInfoUpdated);

	// Unlike desktop system, it is not practical to either look into file systems
	// on Android. And it is simply impossible to "enumerate" asset directories.
	// Therefore we simply return dummy "/" directory.
	const char** getDefaultPluginSearchPaths()
	{
		return default_plugin_search_paths;
	}

	AAPDescriptor** getPluginDescriptorList()
	{
		return plugin_descriptors;
	}
	
	AAPDescriptor* getPluginDescriptor(const char *identifier)
	{
		// TODO: implement
		return NULL;
	}
	
	void updatePluginDescriptorList(const char **searchPaths, bool recursive, bool asynchronousInstantiationAllowed);
	
	AAPInstance* instantiatePlugin(AAPDescriptor *descriptor);
};

class AAPEditor
{
	AAPInstance *owner;

public:

	AAPEditor(AAPInstance *owner)
		: owner(owner)
	{
		// TODO: implement
	}

	void startEditorUI()
	{
		// TODO: implement
	}
};

class AAPInstance
{
	friend class AAPHost;
	
	AAPDescriptor *descriptor;
	AndroidAudioPlugin *plugin;
	AAPHandle *instance;
	const AndroidAudioPluginExtension * const *extensions;
	PluginInstantiationState plugin_state;

	AAPInstance(AAPDescriptor* pluginDescriptor, AndroidAudioPlugin* loadedPlugin)
		: descriptor(pluginDescriptor),
		  plugin(loadedPlugin),
		  instance(NULL),
		  extensions(NULL),
		  plugin_state(PLUGIN_INSTANTIATION_STATE_UNPREPARED)
	{
	}
	
public:

	AAPDescriptor* getPluginDescriptor()
	{
		return descriptor;
	}

	void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock)
	{
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_UNPREPARED);
		
		instance = plugin->instantiate(plugin, sampleRate, extensions);
		plugin->prepare(instance);
		plugin_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void activate()
	{
		if (plugin_state == PLUGIN_INSTANTIATION_STATE_ACTIVE)
			return;
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_INACTIVE);
		
		plugin->activate(instance);
		plugin_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
	}
	
	void deactivate()
	{
		if (plugin_state == PLUGIN_INSTANTIATION_STATE_INACTIVE)
			return;
		assert(plugin_state == PLUGIN_INSTANTIATION_STATE_ACTIVE);
		
		plugin->deactivate(instance);
		plugin_state = PLUGIN_INSTANTIATION_STATE_INACTIVE;
	}
	
	void dispose()
	{
		if (instance != NULL)
			plugin->terminate(instance);
		instance = NULL;
		plugin_state = PLUGIN_INSTANTIATION_STATE_TERMINATED;
	}
	
	void process(AndroidAudioPluginBuffer *buffer, int32_t timeoutInNanoseconds)
	{
		plugin->process(instance, buffer, timeoutInNanoseconds);
	}
	
	double getTailLengthSeconds() const
	{
		// TODO: implement
		return 0;
	}
	
	AAPEditor* createEditor()
	{
		return new AAPEditor(this);
	}
	
	int getNumPrograms()
	{
		// TODO: implement
		return 0;
	}
	
	int getCurrentProgram()
	{
		// TODO: implement
		return 0;
	}
	
	void setCurrentProgram(int index)
	{
		// TODO: implement
	}
	
	const char * getProgramName(int index)
	{
		// TODO: implement
		return NULL;
	}
	
	void changeProgramName(int index, const char * newName)
	{
		// TODO: implement
	}
	
	int32_t getStateSize()
	{
		// TODO: implement
		return 0;
	}
	
	void const* getState()
	{
		// TODO: implement
		return NULL;
	}
	
	void setState(const void* data, int32_t offset, int32_t sizeInBytes)
	{
		// TODO: implement
	}
	
	uint32_t getTailTimeInMilliseconds()
	{
		// TODO: implement
		return 0;
	}
};

} // namespace
