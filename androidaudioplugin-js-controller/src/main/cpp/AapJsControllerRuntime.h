#ifndef AAP_JS_CONTROLLER_RUNTIME_H
#define AAP_JS_CONTROLLER_RUNTIME_H

// NB: only the lightweight choc_javascript.h is included here. The heavy QuickJS amalgamation
// (choc_javascript_QuickJS.h) is included by exactly one translation unit (the .cpp), so it is
// compiled once.
#include <choc/javascript/choc_javascript.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace aap { class PluginClient; }

namespace aap::js {

// Native -> JVM upcall (implemented in JNI.cpp): bind a plugin's Android service by package name.
// Required before instancing. Returns true on success.
bool jvmConnectService(const std::string& packageName);

// Mirrors uapmd-app's async-job model: long automation scripts (e.g. a connect+instantiate that
// stalls on a binder connection) are started, polled, and cleared out-of-band so a single broadcast
// does not have to block for the whole operation.
struct AutomationJob {
    std::string jobId;
    std::string state{"running"}; // running | completed | failed
    std::string result{"null"};   // JSON
};

// Single embedded choc/QuickJS context that exposes the AAP host vocabulary to JavaScript.
// Not thread-safe by design: all access is serialized through `mutex`, and the Kotlin side runs
// every script on one dedicated executor thread.
class AapJsControllerRuntime {
public:
    static AapJsControllerRuntime& global();

    // Creates the JS context (if needed) and evaluates the embedded aap-api.js facade source.
    // Safe to call repeatedly; only the first call bootstraps.
    void bootstrap(const std::string& apiJsSource);

    // The provider hands over its connected native client pointer (aap::PluginClient*), obtained
    // from NativePluginClient.native. Pass 0 to detach.
    void setClient(int64_t nativeClientHandle);

    // Provider-supplied plugin catalog (discovery happens JVM-side). Stored verbatim and returned
    // by `__aap_get_plugins()` for the JS facade to JSON.parse.
    void setPluginCatalog(const std::string& json);

    // Runs a script synchronously and returns its result serialized as JSON (or "ERROR: ...").
    std::string runScript(const std::string& code);

    // Async job variants.
    std::string startJob(const std::string& code);
    std::string getJob(const std::string& jobId);
    void clearJob(const std::string& jobId);

private:
    void ensureContext();
    void registerBindings();
    aap::PluginClient* requireClient() const; // throws std::runtime_error if not attached

    std::recursive_mutex mutex_;
    std::unique_ptr<choc::javascript::Context> context_;
    bool bootstrapped_{false};
    std::string apiJsSource_;

    aap::PluginClient* client_{nullptr};
    std::string pluginCatalogJson_{"[]"};

    std::mutex jobsMutex_;
    std::map<std::string, std::shared_ptr<AutomationJob>> jobs_;
    uint64_t nextJobId_{1};
};

} // namespace aap::js

#endif // AAP_JS_CONTROLLER_RUNTIME_H
