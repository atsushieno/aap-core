
#include "aap/core/host/extension-registry.h"

namespace aap {

//-----------------------------------

AndroidAudioPluginServiceExtension *
PluginExtensionServiceRegistry::create(const char *uri, AndroidAudioPluginHost *host,
                                AndroidAudioPluginExtension *extensionInstance) {
    assert(false); // FIXME: implement
    return nullptr;
}

} // namespace aap
