
#include <string>
#include <vector>
#include <sys/utsname.h>
#include "../include/aap/android-audio-plugin-host.hpp"

namespace aap
{

class GenericDesktopPluginHostPAL : public PluginHostPAL
{
    std::vector<std::string> ret{};
#ifdef WINDOWS
    char sep = ';';
#else
    char sep = ':';
#endif

public:
    std::vector<PluginInformation*> loadPluginListCache() override {
        assert(false); // FIXME: implement
    }

    std::vector<PluginInformation*> getInstalledPlugins() override {
	    assert(false); // FIXME: implement
    }

    std::vector<std::string> getPluginPaths() {
        if (ret.size() > 0)
            return ret;

        auto aappath = getenv("AAP_PLUGIN_PATH");
        if (aappath != nullptr) {
            std::string ap{aappath};
            int idx = 0;
            while (true) {
                int si = ap.find_first_of(sep, idx);
                if (si < 0)
                    break;
                ret.emplace_back(std::string(ap.substr(idx, si)));
                idx = si + 1;
            }
            return ret;
        }

        struct utsname un{};
        uname(&un);
#ifdef WINDOWS
        ret.emplace_back("c:\\AAP Plugins");
        ret.emplace_back("c:\\Program Files[\\AAP Plugins");
#else
        if (strcmp(un.sysname, "Darwin") == 0) {
            ret.emplace_back(std::string(getenv("HOME")) + "/Library/Audio/Plug-ins/AAP");
            ret.emplace_back("/Library/Audio/Plug-ins/AAP");
        } else {
            ret.emplace_back(std::string(getenv("HOME")) + "/.aap");
            ret.emplace_back("/usr/local/lib/aap");
            ret.emplace_back("/usr/lib/aap");
        }
#endif

        return ret;
    }
};

GenericDesktopPluginHostPAL pal_instance{};

PluginHostPAL* getPluginHostPAL()
{
    return &pal_instance;
}

} // namespace aap