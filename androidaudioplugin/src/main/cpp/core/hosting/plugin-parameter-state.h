#ifndef AAP_CORE_HOSTING_PLUGIN_PARAMETER_STATE_H
#define AAP_CORE_HOSTING_PLUGIN_PARAMETER_STATE_H

#include <cstdint>

namespace aap {

class PluginInstance;

namespace internal {

void cleanupParameterState(PluginInstance& instance);
void rebuildParameterIndexAndValues(PluginInstance& instance);
void updateParameterValueCacheFromOutputBuffer(PluginInstance& instance, void* buffer);
bool updateCachedParameterValueById(PluginInstance& instance, int32_t parameterId, double plainValue);
double getParameterValue(PluginInstance& instance, int32_t index);
void handleParameterLayoutChanged(PluginInstance& instance);

}
}

#endif
