
#include <string>
#include <vector>
#include <filesystem>
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
        std::vector<PluginInformation *> ret{};
        for (auto p : getPluginPaths())
            iterateDirectory(p, ret);
        return ret;
    }

    void iterateDirectory(std::filesystem::path path, std::vector<PluginInformation *> &ret)
    {
        for(const auto &entry : std::filesystem::directory_iterator(path)) {
            if (!entry.is_directory() && entry.path().filename() == "aap_metadata.xml") {
                for (auto x : PluginInformation::parsePluginDescriptor("", entry.path().c_str()))
                    ret.emplace_back(x);
            }
            else if (entry.is_directory())
                iterateDirectory(entry, ret);
        }
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
                if (si < 0) {
                    if (idx < ap.size())
                        ret.emplace_back(std::string(ap.substr(idx, ap.size())));
                    break;
                }
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