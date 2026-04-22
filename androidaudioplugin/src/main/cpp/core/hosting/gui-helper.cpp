#include "aap/core/host/gui-helper.h"
#include <algorithm>

namespace aap {

GuiViewportConfiguration GuiViewportConfiguration::clamp() const {
    GuiViewportConfiguration clamped = *this;
    clamped.viewportWidth = std::max<int32_t>(0, clamped.viewportWidth);
    clamped.viewportHeight = std::max<int32_t>(0, clamped.viewportHeight);
    clamped.contentWidth = std::max(clamped.viewportWidth, clamped.contentWidth);
    clamped.contentHeight = std::max(clamped.viewportHeight, clamped.contentHeight);
    auto maxScrollX = std::max<int32_t>(0, clamped.contentWidth - clamped.viewportWidth);
    auto maxScrollY = std::max<int32_t>(0, clamped.contentHeight - clamped.viewportHeight);
    clamped.scrollX = std::clamp(clamped.scrollX, 0, maxScrollX);
    clamped.scrollY = std::clamp(clamped.scrollY, 0, maxScrollY);
    return clamped;
}

GuiSize getGuiPreferredSizeOrFallback(
    RemotePluginInstance* instance,
    int32_t fallbackWidth,
    int32_t fallbackHeight) {
    GuiSize size{fallbackWidth, fallbackHeight};
    if (!instance)
        return size;
    instance->prepareSurfaceControlForRemoteNativeUI();
    int32_t preferredWidth = 0;
    int32_t preferredHeight = 0;
    if (instance->getRemoteNativeViewPreferredSize(preferredWidth, preferredHeight)) {
        if (preferredWidth > 0)
            size.width = preferredWidth;
        if (preferredHeight > 0)
            size.height = preferredHeight;
    }
    return size;
}

}
