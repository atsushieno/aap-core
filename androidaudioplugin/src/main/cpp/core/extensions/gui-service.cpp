#include "gui-service.h"
#include "../AAPJniFacade.h"


namespace aap {

void GuiPluginServiceExtension::onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
               int32_t opcode)  {
#if ANDROID
    switch (opcode) {
        case OPCODE_CREATE: {
            auto len = *(int32_t *) extensionInstance->data;
            assert(len < AAP_MAX_PLUGIN_ID_SIZE);
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
            AAPJniFacade::getInstance()->createGuiViaJni(gi, [pluginId, instanceId, gi, this, plugin]() {
                if (!gi->lastError.empty())
                    return;
                withGuiExtension<int32_t>(plugin, 0, [&](aap_gui_extension_t *ext,
                                                         AndroidAudioPluginExtensionTarget target) {
                    if (ext && ext->create) {
                        volatile void* v = gi->view;
                        gi->internalGuiInstanceId = ext->create(target, pluginId, instanceId, (void*) v);
                    }
                    else
                        aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAP", "`gui` extension or `create` member does not exist. Skipping `create()`.");
                });
            });
        }
            break;
        case OPCODE_SHOW:
        case OPCODE_HIDE:
        case OPCODE_DESTROY: {
            auto guiInstanceId = *(int32_t *) extensionInstance->data;
            auto gi = (GuiInstance*) extensionInstance->local_data;
            if (gi->externalGuiInstanceId != guiInstanceId) {
                gi->lastError.clear();
                gi->lastError = std::string{"Invalid operation: GUI state mismatch. Expected "}
                    + std::to_string(gi->externalGuiInstanceId) + " but got " + std::to_string(guiInstanceId) + ".";
                break;
            }

            switch (opcode) {
                case OPCODE_SHOW:
                    AAPJniFacade::getInstance()->showGuiViaJni(gi, [gi, plugin, this]() {
                        if (!gi->lastError.empty())
                            return;
                        withGuiExtension<int32_t>(plugin, 0, [&](aap_gui_extension_t *ext,
                                                                 AndroidAudioPluginExtensionTarget target) {
                            if (ext && ext->show)
                                ext->show(target, gi->internalGuiInstanceId);
                            else
                                aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAP",
                                             "`gui` extension or `show` member does not exist. Skipping `show()`.");
                        });
                    });
                    break;
                case OPCODE_HIDE:
                    AAPJniFacade::getInstance()->hideGuiViaJni(gi, [gi, plugin, this]() {
                        if (!gi->lastError.empty())
                            return;
                        withGuiExtension<int32_t>(plugin, 0, [&](aap_gui_extension_t *ext,
                                                                 AndroidAudioPluginExtensionTarget target) {
                            if (ext && ext->hide)
                                ext->hide(target, gi->internalGuiInstanceId);
                            else
                                aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAP",
                                             "`gui` extension or `hide` member does not exist. Skipping `hide()`.");
                        });
                    });
                    break;
                case OPCODE_DESTROY:
                    AAPJniFacade::getInstance()->destroyGuiViaJni(gi, [gi, plugin, this]() {
                        if (!gi->lastError.empty())
                            return;
                        withGuiExtension<int32_t>(plugin, 0, [&](aap_gui_extension_t *ext,
                                                                 AndroidAudioPluginExtensionTarget target) {
                            if (ext && ext->destroy)
                                ext->destroy(target, gi->internalGuiInstanceId);
                            else
                                aap::a_log_f(AAP_LOG_LEVEL_INFO, "AAP",
                                             "`gui` extension or `destroy` member does not exist. Skipping `destroy()`.");
                            delete gi;
                        });
                    });
                    break;
            }
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
