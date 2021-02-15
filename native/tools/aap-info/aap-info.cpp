#include <iostream>
#include <ostream>
#include "aap/audio-plugin-host.h"
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "USAGE: aap-info [plugin_identifier]" << std::endl;
        return 1;
    }

    aap::PluginHostManager manager{};

    auto info = manager.getPluginInformation(argv[1]);
    if (!info) {
        std::cerr << "aap-info: plugin" << argv[1] << " not found." << std::endl;
        return 2;
    }
    std::cout << "Plugin Identifier:\t" << argv[1] << std::endl;
    std::cout << "Name:\t\t\t" << info->getDisplayName() << std::endl;
    std::cout << "Plugin Package Name:\t" << info->getPluginPackageName() << std::endl;
    std::cout << "Plugin Local Name:\t" << info->getPluginLocalName() << std::endl;
    std::cout << "Manufacturer:\t\t" << info->getManufacturerName() << std::endl;
    std::cout << "Version:\t\t\t" << info->getVersion() << std::endl;
    std::cout << "Primary Category:\t" << info->getPrimaryCategory() << std::endl;
    std::cout << "Library:\t\t" << info->getLocalPluginSharedLibrary() << std::endl;
    std::cout << "Library Entrypoint:\t" << info->getLocalPluginLibraryEntryPoint() << std::endl;

    std::cout << "Required Extensions:" << std::endl;
    size_t nExtRequired = info->getNumRequiredExtensions();
    for (size_t n = 0; n < nExtRequired; n++) {
        std::cout << "  - " << info->getRequiredExtension(n) << std::endl;
    }
    std::cout << "Optional Extensions:" << std::endl;
    size_t nExtOptional = info->getNumOptionalExtensions();
    for (size_t n = 0; n < nExtOptional; n++) {
        std::cout << "  - " << info->getOptionalExtension(n) << std::endl;
    }

    std::cout << "Ports:" << std::endl;
    size_t nPorts = info->getNumPorts();
    for (size_t n = 0; n < nPorts; n++) {
        auto port = info->getPort(n);
        std::cout << "  Port " << n << ":" << std::endl;
        std::cout << "    - Name: " << port->getName() << std::endl;
        std::cout << "    - Content Type: " << port->getContentType() << std::endl;
        std::cout << "    - Direction: " << port->getPortDirection() << std::endl;
        std::cout << "    - Default: " << port->getDefaultValue() << std::endl;
        std::cout << "    - Min Value: " << port->getMinimumValue() << std::endl;
        std::cout << "    - Max Value: " << port->getMaximumValue() << std::endl;
    }

    auto file = info->getLocalPluginSharedLibrary();
    auto metadataFullPath = info->getMetadataFullPath();
    std::cout << "Metadata fullpath:\t" << metadataFullPath << std::endl;
    if (!metadataFullPath.empty()) {
        size_t idx = metadataFullPath.find_last_of('/');
        if (idx > 0) {
            auto soFullPath = metadataFullPath.substr(0, idx + 1) + file;
            struct stat st;
            if (stat(soFullPath.c_str(), &st) == 0)
                file = soFullPath;
            std::cout << "so fullpath:\t\t" << soFullPath << std::endl;
        }
    }
    auto entrypoint = info->getLocalPluginLibraryEntryPoint();
    auto dl = dlopen(file.length() > 0 ? file.c_str() : "libandroidaudioplugin.so", RTLD_LAZY);
    if (dl == nullptr)
        std::cout << "AAP library " << file << " could not be loaded." << std::endl;
    else
        dlclose(dl);
}

