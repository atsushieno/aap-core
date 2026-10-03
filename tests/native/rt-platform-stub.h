#pragma once
// The host-only integration fixture replaces Android JNI/UI and memory setup.
// PluginInstance, AAPXS dispatch, serialization, queues and cache code remain production code.
#define AAP_CORE_AAPJNIFACADE_H
#include <cstdint>
#include <string>
namespace aap {
class AAPJniFacade {
public:
    static AAPJniFacade* getInstance() { static AAPJniFacade facade; return &facade; }
    int32_t getMidiSettingsFromLocalConfig(std::string) { return 0; }
    void* createSurfaceControl() { return nullptr; }
    void disposeSurfaceControl(void*) {}
    void showSurfaceControlView(void*) {}
    void hideSurfaceControlView(void*) {}
};
}
