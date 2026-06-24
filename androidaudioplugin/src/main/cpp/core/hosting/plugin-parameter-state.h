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
void updateParameterValueCacheFromOutputBuffer(PluginInstance& instance, void* buffer);
bool updateCachedParameterValueById(PluginInstance& instance, int32_t parameterId, double plainValue);
double getParameterValue(PluginInstance& instance, int32_t index);
void handleParameterLayoutChanged(PluginInstance& instance);

}
}

#endif
