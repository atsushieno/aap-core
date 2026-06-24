#ifndef AAP_CORE_HOSTING_PLUGIN_PARAMETER_STATE_H
#define AAP_CORE_HOSTING_PLUGIN_PARAMETER_STATE_H

#include <cstdint>
#include <functional>

namespace aap {

class PluginInstance;
class RemotePluginInstance;

namespace internal {

void cleanupParameterState(PluginInstance& instance);
void rebuildParameterIndexAndValues(PluginInstance& instance);
// Reindexes ids and preserves values from the already-populated cached_parameters (no rescan).
void reindexParameterValues(PluginInstance& instance);
// Non-blocking host-side parameter-layout rebuild: asynchronously rescans the plugin (count,
// each parameter, each enumeration), rebuilds cached_parameters and the value index (preserving
// known values by id), then invokes onComplete(true); on failure invokes onComplete(false) and
// leaves the cache untouched. Triggered by the parameters receiver on notify_parameters_changed.
void rebuildParameterListAsync(RemotePluginInstance& instance, std::function<void(bool ok)> onComplete);
void updateParameterValueCacheFromOutputBuffer(PluginInstance& instance, void* buffer);
bool updateCachedParameterValueById(PluginInstance& instance, int32_t parameterId, double plainValue);
double getParameterValue(PluginInstance& instance, int32_t index);
void handleParameterLayoutChanged(PluginInstance& instance);

}
}

#endif
