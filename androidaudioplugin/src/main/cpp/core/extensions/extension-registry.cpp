
#include "aap/core/host/extension-registry.h"


namespace aap {

//-----------------------------------

PluginServiceExtension* PluginExtensionServiceRegistry::getByUri(const char *uri) {
    for (auto &e : extension_services)
        if (strcmp(e->asTransient()->uri, uri) == 0)
            return e.get();
    return nullptr;
}

} // namespace aap
