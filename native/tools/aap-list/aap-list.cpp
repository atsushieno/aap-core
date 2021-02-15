#include <iostream>
#include "aap/audio-plugin-host.h"

int main(int argc, char** argv) {
    aap::PluginHostManager manager{};

    size_t numPlugins = manager.getNumPluginInformation();
    for (size_t n = 0; n < numPlugins; n++) {
        auto info = manager.getPluginInformation(n);
        std::cout << info->getPluginID() << std::endl;
    }
    std::cout << "Total " << numPlugins << " plugins found." << std::endl;
}

