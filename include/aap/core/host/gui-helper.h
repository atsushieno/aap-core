#ifndef AAP_CORE_HOST_GUI_HELPER_H
#define AAP_CORE_HOST_GUI_HELPER_H

#include "plugin-instance.h"

namespace aap {

    struct GuiSize {
        int32_t width{0};
        int32_t height{0};
    };

    struct GuiViewportConfiguration {
        int32_t viewportWidth{0};
        int32_t viewportHeight{0};
        int32_t contentWidth{0};
        int32_t contentHeight{0};
        int32_t scrollX{0};
        int32_t scrollY{0};

        GuiViewportConfiguration clamp() const;
    };

    GuiSize getGuiPreferredSizeOrFallback(
        RemotePluginInstance* instance,
        int32_t fallbackWidth,
        int32_t fallbackHeight);
}

#endif // AAP_CORE_HOST_GUI_HELPER_H
