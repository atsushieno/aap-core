
#include "../include/aap/android-audio-plugin-host.hpp"

namespace aap
{

class LinuxPluginHostPAL : public PluginHostPAL
{
public:
    PluginInformation** getInstalledPlugins() override {
	    assert(false); // FIXME: implement
    }
};

LinuxPluginHostPAL android_pal_instance{};

PluginHostPAL* getPluginHostPAL()
{
    return &android_pal_instance;
}

} // namespace aap