#pragma once
// Host-only preference-store substitute. Production MIDI request/reply handlers are linked;
// the Android JNI preference lookup is the sole replacement in this test binary.
#define AAP_CORE_AAPJNIFACADE_H
#include <cstdint>
#include <string>
namespace aap {
class AAPJniFacade {
public:
    static AAPJniFacade* getInstance();
    int32_t getMidiSettingsFromLocalConfig(std::string pluginId);
};
}
