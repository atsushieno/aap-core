
#include <string>
#include <vector>
#include <dirent.h>
#include <libgen.h>
#include <sys/utsname.h>
#include "../include/aap/android-audio-plugin-host.hpp"

namespace aap
{

class GenericDesktopPluginHostPAL : public PluginHostPAL
{
    std::vector<std::string> cached_search_paths{};
#ifdef WINDOWS
    char sep = ';';
#else
    char sep = ':';
#endif

public:
	std::vector<PluginInformation*> getPluginsFromMetadataPaths(std::vector<std::string>& aapMetadataPaths) override {
		std::vector<PluginInformation *> ret{};
		for (auto p : aapMetadataPaths) {
			char* pc = strdup(p.c_str());
			// FIXME: we need some semantics on packageName and localName on desktop
			for (auto x : PluginInformation::parsePluginMetadataXml(nullptr, nullptr, pc))
				ret.emplace_back(x);
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

    std::vector<std::string> getPluginPaths() override {
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
};

GenericDesktopPluginHostPAL pal_instance{};

PluginHostPAL* getPluginHostPAL()
{
    return &pal_instance;
}

} // namespace aap
