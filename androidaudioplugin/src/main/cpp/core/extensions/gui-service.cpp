#include "gui-service.h"
#include "../AAPJniFacade.h"


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

            std::string pluginIdString{pluginId};

            auto gi = new GuiInstance();
            gi->externalGuiInstanceId = getGuiInstanceSerial();
            gi->pluginId = pluginId;
            gi->instanceId = instanceId;
            extensionInstance->local_data = gi;
            *(int32_t *) extensionInstance->data = gi->externalGuiInstanceId;
            // It is asynchronously dispatched to non-RT loop.
            AAPJniFacade::getInstance()->createGuiViaJni(gi, [&]() {
                withGuiExtension<int32_t>(plugin, 0, [&](aap_gui_extension_t *ext,
                                                         AndroidAudioPluginExtensionTarget target) {
                    gi->internalGuiInstanceId = ext->create(target, pluginId, instanceId, gi->view);
                });
            });
        }
            break;
        case OPCODE_SHOW:
        case OPCODE_HIDE:
        case OPCODE_DESTROY: {
            withGuiExtension<int32_t>(plugin, 0, [&](aap_gui_extension_t *ext,
                                                     AndroidAudioPluginExtensionTarget target) {
                auto guiInstanceId = *(int32_t *) extensionInstance->data;
                auto gi = (GuiInstance*) extensionInstance->local_data;

                switch (opcode) {
                    case OPCODE_SHOW:
                        AAPJniFacade::getInstance()->showGuiViaJni(gi, [&]() { ext->show(target, guiInstanceId); }, gi->view);
                        break;
                    case OPCODE_HIDE:
                        AAPJniFacade::getInstance()->hideGuiViaJni(gi, [&]() { ext->hide(target, guiInstanceId); }, gi->view);
                        break;
                    case OPCODE_DESTROY:
                        AAPJniFacade::getInstance()->destroyGuiViaJni(gi, [&]() {
                            ext->destroy(target, guiInstanceId);
                            delete gi;
                        }, gi->view);
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
