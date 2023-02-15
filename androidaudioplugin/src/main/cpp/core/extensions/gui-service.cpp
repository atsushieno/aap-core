#include "gui-service.h"



namespace aap {

void GuiPluginServiceExtension::onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
               int32_t opcode)  {
#if ANDROID
    switch (opcode) {
        case OPCODE_CREATE: {
            auto len = *(int32_t *) extensionInstance->data;
            assert(len < MAX_PLUGIN_ID_SIZE);
            char *pluginId = (char *) calloc(len, 1);
            strncpy(pluginId, (const char *) ((int32_t *) extensionInstance->data + 1),
                    len);
            auto instanceId = *((int32_t *) extensionInstance->data + 1 + len);

            // Unlike other functions, create() at AAPXS level first looks for the target
            // AudioPluginViewFactory class from aap_metadata.xml, and starts `AudioPluginView`
            // instantiation step. The factory might then continue to View instantiation, or
            // invokes native `create()` extension function to let native code create View
            // and set as content to `AudioPluginView`.
            void* view = createGuiViaJni(pluginId, instanceId);

            withGuiExtension<int32_t>(plugin, 0, [=](aap_gui_extension_t *ext,
                                                     AndroidAudioPluginExtensionTarget target) {
                // This nullptr must be actually a jobject of AudioPluginView.
                auto guiInstanceId = ext->create(target, pluginId, instanceId, view);
                *(int32_t *) extensionInstance->data = guiInstanceId;
            });
        }
            break;
        case OPCODE_SHOW:
        case OPCODE_HIDE:
        case OPCODE_DESTROY: {
            withGuiExtension<int32_t>(plugin, 0, [=](aap_gui_extension_t *ext,
                                                     AndroidAudioPluginExtensionTarget target) {
                auto guiInstanceId = *(int32_t *) extensionInstance->data;
                switch (opcode) {
                    case OPCODE_SHOW:
                        ext->show(target, guiInstanceId);
                        break;
                    case OPCODE_HIDE:
                        ext->hide(target, guiInstanceId);
                        break;
                    case OPCODE_DESTROY:
                        ext->destroy(target, guiInstanceId);
                        break;
                }
            });
        }
            break;
        case OPCODE_RESIZE: {
            auto guiInstanceId = *(int32_t *) extensionInstance->data;
            auto width = *((int32_t *) extensionInstance->data + 1);
            auto height = *((int32_t *) extensionInstance->data + 1);
            withGuiExtension<int32_t>(plugin, 0, [=](aap_gui_extension_t *ext,
                                                     AndroidAudioPluginExtensionTarget target) {
                auto result = ext->resize(target, guiInstanceId, width, height);
                *(int32_t *) extensionInstance->data = result;
            });
        }
            break;
        default:
            assert(false); // should not happen
    }
#else
    puts("gui extension is not supported on desktop.");
    assert(false);
#endif
}

}
