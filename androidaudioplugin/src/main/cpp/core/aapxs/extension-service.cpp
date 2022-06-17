
#include "aap/core/host/audio-plugin-host.h"
#include "aap/core/aapxs/extension-service.h"

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

bool AAPXSClientInstanceManager::setupAAPXSInstances(std::function<void(AAPXSClientInstance*)> func) {
    auto pluginInfo = getPluginInformation();
    for (int i = 0, n = pluginInfo->getNumExtensions(); i < n; i++) {
        auto info = pluginInfo->getExtension(i);
        auto feature = getExtensionFeature(info.uri.c_str());
        if (feature == nullptr && info.required) {
            aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP", "The extension '%s' is declared as required, but was actually not found.", info.uri.c_str());
            return false;
        }
        if (feature) {
            auto aapxsInstance = setupAAPXSInstance(feature);
            if (!aapxsInstance) {
                aap::a_log_f(AAP_LOG_LEVEL_ERROR, "AAP", "Failed to set up AAPXS Proxy for extension '%s'. There may be corresponding failure log from the plugin too.", feature->uri);
                return false;
            }
            else
                func(aapxsInstance);
        }
    }
    return true;
}

} // namespace aap
