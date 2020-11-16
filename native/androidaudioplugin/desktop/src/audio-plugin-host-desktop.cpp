
#include <string>
#include <vector>
#include <dirent.h>
#include <libgen.h>
#include <sys/utsname.h>
#include "../include/aap/android-audio-plugin-host.hpp"
#include "audio-plugin-host-desktop.hpp"

namespace aap
{

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
