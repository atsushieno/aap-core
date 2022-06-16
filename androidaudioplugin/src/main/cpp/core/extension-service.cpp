
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/host/extension-service.h"

namespace aap {

AAPXSProxyContext AAPXSClientInstanceManager::getExtensionProxy(const char* uri) {
    auto aapxsClientInstance = getInstanceFor(uri);
    if (!aapxsClientInstance) {
        aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAP", "AAPXS Proxy for extension '%s' is not found", uri);
        return AAPXSProxyContext{nullptr, nullptr, nullptr};
    }
    auto feature = getExtensionFeature(uri);
    assert(strlen(feature->uri) > 0);
    assert(feature->as_proxy != nullptr);
    return feature->as_proxy(feature, aapxsClientInstance);
}

void AAPXSClientInstanceManager::setupAAPXSInstances(std::function<void(AAPXSClientInstance*)> func) {
    auto pluginInfo = getPluginInformation();
    for (int i = 0, n = pluginInfo->getNumExtensions(); i < n; i++) {
        auto info = pluginInfo->getExtension(i);
        auto feature = getExtensionFeature(info.uri.c_str());
        assert (feature != nullptr || info.required);
        func(setupAAPXSInstance(feature));
    }
}

} // namespace aap
