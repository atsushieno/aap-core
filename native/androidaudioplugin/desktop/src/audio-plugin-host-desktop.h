#ifndef ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_DESKTOP_HPP
#define ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_DESKTOP_HPP
#ifndef ANDROID

#include <dirent.h>
#include "aap/audio-plugin-host.h"

namespace aap {

void aap_parse_plugin_descriptor_into(const char* pluginPackageName, const char* pluginLocalName, const char* xmlfile, std::vector<PluginInformation*>& plugins);

class GenericDesktopPluginHostPAL : public PluginHostPAL
{
    std::vector<std::string> cached_search_paths{};
#ifdef WINDOWS
    char sep = ';';
#else
    char sep = ':';
#endif

public:
	std::string getRemotePluginEntrypoint() override { return "GetDesktopAudioPluginFactoryClientBridge"; }

	std::vector<PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) override {
		std::vector<PluginInformation *> ret{};
		for (auto p : aapMetadataPaths) {
			char* pc = strdup(p.c_str());
			aap_parse_plugin_descriptor_into(nullptr, nullptr, pc, ret);
			free(pc);
		}
		return ret;
	}

	std::string getFullPath(std::string path, dirent* entry) {
		return path + (path[path.length() - 1] == '/' ? "" : "/") + entry->d_name;
	}

	void getAAPMetadataPaths(std::string path, std::vector<std::string>& results) override
	{
		auto cpath = strdup(path.c_str());
    	DIR* dir = opendir(cpath);
    	if(dir != nullptr) {
			while (auto entry = readdir(dir)) {
				if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
					continue;
				if (strcasecmp(entry->d_name, "aap_metadata.xml") == 0)
					results.emplace_back(getFullPath(path, entry));
				else if (entry->d_type == DT_DIR)
					getAAPMetadataPaths(getFullPath(path, entry).c_str(), results);
			}
		}
		free(cpath);
	}

    std::vector<std::string> getPluginPaths() override;
};

}


#endif // ANDROID

#endif // ANDROIDAUDIOPLUGINFRAMEWORK_ANDROID_AUDIO_PLUGIN_HOST_DESKTOP_HPP
