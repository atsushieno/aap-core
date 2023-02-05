
#ifndef AAP_CORE_GUI_SERVICE_H
#define AAP_CORE_GUI_SERVICE_H

#include <cstdint>
#include <functional>
#include "aap/android-audio-plugin.h"
#include "aap/unstable/aapxs.h"
#include "aap/ext/gui.h"
#include "aap/core/aapxs/extension-service.h"
#include "extension-service-impl.h"

namespace aap {

// implementation details
    const int32_t OPCODE_SHOW = 0;
    const int32_t OPCODE_HIDE = 1;


    class GuiPluginClientExtension : public PluginClientExtensionImplBase {
        class Instance {
            friend class GuiPluginClientExtension;

            aap_gui_extension_t proxy{};

            GuiPluginClientExtension *owner;
            AAPXSClientInstance* aapxsInstance;

            static int32_t internalShow(AndroidAudioPluginExtensionTarget target) {
                return ((Instance *) target.aapxs_context)->show();
            }

            static void internalHide(AndroidAudioPluginExtensionTarget target) {
                return ((Instance *) target.aapxs_context)->hide();
            }

        public:
            Instance(GuiPluginClientExtension *owner, AAPXSClientInstance *clientInstance)
                    : owner(owner)
            {
                aapxsInstance = clientInstance;
            }

            int32_t show() {
                clientInvokePluginExtension(OPCODE_SHOW);
                return *((int32_t*) aapxsInstance->data);
            }

            void hide() {
                clientInvokePluginExtension(OPCODE_HIDE);
            }

            void clientInvokePluginExtension(int32_t opcode) {
                owner->clientInvokePluginExtension(aapxsInstance, opcode);
            }

            AAPXSProxyContext asProxy() {
                proxy.show = internalShow;
                proxy.hide = internalHide;
                return AAPXSProxyContext{aapxsInstance, this, &proxy};
            }
        };

// FIXME: This tells there is maximum # of instances - we need some better method to retain pointers
//  to each Instance that at least lives as long as AAPXSClientInstance lifetime.
//  (Should we add `addDisposableListener` at AAPXSClient to make it possible to free
//  this Instance at plugin instance disposal? Maybe when if 1024 for instances sounds insufficient...)
#define GUI_AAPXS_MAX_INSTANCE_COUNT 1024

        std::unique_ptr<Instance> instances[GUI_AAPXS_MAX_INSTANCE_COUNT]{};
        std::map<int32_t,int32_t> instance_map{}; // map from instanceId to the index of the Instance in `instances`.

    public:
        GuiPluginClientExtension()
                : PluginClientExtensionImplBase() {
        }

        AAPXSProxyContext asProxy(AAPXSClientInstance *clientInstance) override {
            size_t last = 0;
            for (; last < GUI_AAPXS_MAX_INSTANCE_COUNT; last++) {
                if (instances[last] == nullptr)
                    break;
                if (instances[last]->aapxsInstance == clientInstance)
                    return instances[instance_map[clientInstance->plugin_instance_id]]->asProxy();
            }
            instances[last] = std::make_unique<Instance>(this, clientInstance);
            instance_map[clientInstance->plugin_instance_id] = (int32_t) last;
            return instances[last]->asProxy();
        }
    };

    class GuiPluginServiceExtension : public PluginServiceExtensionImplBase {

        template<typename T>
        void withGuiExtension(AndroidAudioPlugin* plugin,
                                                             T defaultValue,
                                                             std::function<void(aap_gui_extension_t *, AndroidAudioPluginExtensionTarget)> func) {
            // This instance->getExtension() should return an extension from the loaded plugin.
            assert(plugin);
            auto guiExtension = (aap_gui_extension_t *) plugin->get_extension(plugin, AAP_GUI_EXTENSION_URI);
            if (guiExtension)
                // We don't need context for service side.
                func(guiExtension, AndroidAudioPluginExtensionTarget{plugin, nullptr});
        }

    public:
        GuiPluginServiceExtension()
                : PluginServiceExtensionImplBase(AAP_GUI_EXTENSION_URI) {
        }

        // invoked by AudioPluginService
        void onInvoked(AndroidAudioPlugin* plugin, AAPXSServiceInstance *extensionInstance,
                       int32_t opcode) override {
            switch (opcode) {
                case OPCODE_SHOW:
                case OPCODE_HIDE:
                    auto len = *(int32_t*) extensionInstance->data;
                    assert(len < MAX_PLUGIN_ID_SIZE);
                    char* pluginId = (char*) calloc(len, 1);
                    strncpy(pluginId, (const char*) ((int32_t*) extensionInstance->data + 1), len);

                    withGuiExtension<int32_t>(plugin, 0, [=](aap_gui_extension_t *ext,
                                                               AndroidAudioPluginExtensionTarget target) {
                        if (opcode == OPCODE_SHOW)
                            ext->show(target);
                        else
                            ext->hide(target);
                    });
                    break;
            }
            assert(false); // should not happen
        }
    };


    class GuiExtensionFeature : public PluginExtensionFeatureImpl {
        std::unique_ptr<PluginClientExtensionImplBase> client;
        std::unique_ptr<PluginServiceExtensionImplBase> service;

    public:
        GuiExtensionFeature()
                : PluginExtensionFeatureImpl(AAP_GUI_EXTENSION_URI, false, sizeof(aap_gui_extension_t)),
                  client(std::make_unique<GuiPluginClientExtension>()),
                  service(std::make_unique<GuiPluginServiceExtension>()) {
        }

        PluginClientExtensionImplBase* getClient() { return client.get(); }
        PluginServiceExtensionImplBase* getService() { return service.get(); }
    };

} // namespace aap


#endif //AAP_CORE_GUI_SERVICE_H
