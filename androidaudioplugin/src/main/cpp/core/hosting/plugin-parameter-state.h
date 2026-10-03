#ifndef AAP_CORE_HOSTING_PLUGIN_PARAMETER_STATE_H
#define AAP_CORE_HOSTING_PLUGIN_PARAMETER_STATE_H

#include <cstdint>
#include <functional>

namespace aap {

class PluginInstance;
class RemotePluginInstance;
class ParameterInformation;

namespace internal {

void cleanupParameterState(PluginInstance& instance);
void rebuildParameterIndexAndValues(PluginInstance& instance);
// Reindexes ids and preserves values from the already-populated cached_parameters (no rescan).
void reindexParameterValues(PluginInstance& instance);
void updateParameterValueCacheFromOutputBuffer(PluginInstance& instance, void* buffer);
// Records host-originated parameter changes (AAP parameter SysEx8 in a MIDI2 input buffer).
void updateParameterValueCacheFromInputBuffer(PluginInstance& instance, void* buffer);
bool updateCachedParameterValueById(PluginInstance& instance, int32_t parameterId, double plainValue);
double getParameterValue(PluginInstance& instance, int32_t index);
// Records a value that the host has just sent (or queued) to the plugin, by parameter index.
// Hosts that are not processing yet would otherwise read back the stale value until process() runs.
void setCachedParameterValue(PluginInstance& instance, int32_t index, double plainValue);
void handleParameterLayoutChanged(PluginInstance& instance);

// ---- Plugin-initiated parameter layout changes (aap-core#130)

// Rescans the parameter list on a worker thread. RT-safe; coalesced per instance.
void requestParameterLayoutRefresh(PluginInstance& instance);
// Service side: the instance can be rescanned once its standard extensions are set up.
void setParameterLayoutRefreshReady(PluginInstance& instance);
// Client side: invoked on the worker thread after each refresh, after parametersChangedHandler.
void setParameterLayoutChangedListener(RemotePluginInstance& instance, std::function<void()> listener);
// PluginHost::destroyInstance() calls them before and after `delete`.
void closeParameterLayoutRefresh(PluginInstance& instance);
void forgetParameterLayoutRefresh(PluginInstance& instance);

// Blocking AAPXS calls must not wait for SysEx8 replies, which only arrive while audio is processed.
class ScopedBinderOnlyAAPXS {
    static thread_local bool active;
    bool previous;
public:
    ScopedBinderOnlyAAPXS() : previous(active) { active = true; }
    ~ScopedBinderOnlyAAPXS() { active = previous; }
    static bool isActive() { return active; }
};

// Consistent with a concurrent refresh, unlike the inline getters in the public header.
// Returns null if out of range; the pointer stays valid for the lifetime of the instance.
int32_t getParameterCountSafely(PluginInstance& instance);
const ParameterInformation* getParameterSafely(PluginInstance& instance, int32_t index);

}
}

#endif
