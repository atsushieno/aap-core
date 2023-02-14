
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
    const int32_t OPCODE_CREATE = 0;
    const int32_t OPCODE_SHOW = 1;
    const int32_t OPCODE_HIDE = 2;
    const int32_t OPCODE_RESIZE = 3;
    const int32_t OPCODE_DESTROY = 4;


    class GuiPluginClientExtension : public PluginClientExtensionImplBase {
        class Instance {
            friend class GuiPluginClientExtension;

            aap_gui_extension_t proxy{};

            GuiPluginClientExtension *owner;
            AAPXSClientInstance* aapxsInstance;

            static aap_gui_instance_id internalCreate(AndroidAudioPluginExtensionTarget target, const char* pluginId, int32_t instanceId) {
                return ((Instance *) target.aapxs_context)->create(pluginId, instanceId);
            }

            static int32_t internalShow(AndroidAudioPluginExtensionTarget target, aap_gui_instance_id guiInstanceId) {
                return ((Instance *) target.aapxs_context)->show(guiInstanceId);
            }

            static void internalHide(AndroidAudioPluginExtensionTarget target, aap_gui_instance_id guiInstanceId) {
                return ((Instance *) target.aapxs_context)->hide(guiInstanceId);
            }

            static int32_t internalResize(AndroidAudioPluginExtensionTarget target, aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) {
                return ((Instance *) target.aapxs_context)->resize(guiInstanceId, width, height);
            }

            static void internalDestroy(AndroidAudioPluginExtensionTarget target, aap_gui_instance_id guiInstanceId) {
                ((Instance *) target.aapxs_context)->destroy(guiInstanceId);
            }

        public:
            Instance(GuiPluginClientExtension *owner, AAPXSClientInstance *clientInstance)
                    : owner(owner)
            {
                aapxsInstance = clientInstance;
            }

            int32_t create(std::string pluginId, int32_t instanceId) {
                *((int32_t*) aapxsInstance->data) = pluginId.length();
                memcpy((int32_t*) (aapxsInstance->data) + 1, pluginId.c_str(), pluginId.length());
                *((int32_t*) (aapxsInstance->data) + 1 + pluginId.length()) = instanceId;
                clientInvokePluginExtension(OPCODE_CREATE);
                return *((int32_t*) aapxsInstance->data);
            }

            int32_t show(aap_gui_instance_id guiInstanceId) {
                *((int32_t*) aapxsInstance->data) = guiInstanceId;
                clientInvokePluginExtension(OPCODE_SHOW);
                return *((int32_t*) aapxsInstance->data);
            }

            void hide(aap_gui_instance_id guiInstanceId) {
                *((int32_t*) aapxsInstance->data) = guiInstanceId;
                clientInvokePluginExtension(OPCODE_HIDE);
            }

            int32_t resize(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) {
                *((int32_t*) aapxsInstance->data) = guiInstanceId;
                *((int32_t*) aapxsInstance->data + 1) = width;
                *((int32_t*) aapxsInstance->data + 2) = height;
                clientInvokePluginExtension(OPCODE_RESIZE);
                return *((int32_t*) aapxsInstance->data);
            }

            void destroy(aap_gui_instance_id guiInstanceId) {
                *((int32_t*) aapxsInstance->data) = guiInstanceId;
                clientInvokePluginExtension(OPCODE_DESTROY);
            }

            void clientInvokePluginExtension(int32_t opcode) {
                owner->clientInvokePluginExtension(aapxsInstance, opcode);
            }

            AAPXSProxyContext asProxy() {
                proxy.create = internalCreate;
                proxy.show = internalShow;
                proxy.hide = internalHide;
                proxy.resize = internalResize;
                proxy.destroy = internalDestroy;
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
                case OPCODE_CREATE: {
                    auto len = *(int32_t *) extensionInstance->data;
                    assert(len < MAX_PLUGIN_ID_SIZE);
                    char *pluginId = (char *) calloc(len, 1);
                    strncpy(pluginId, (const char *) ((int32_t *) extensionInstance->data + 1),
                            len);
                    auto instanceId = *(int32_t *) extensionInstance->data + 1 + len;

                    // Unlike other functions, create() at AAPXS level first looks for the target
                    // AudioPluginViewFactory class from aap_metadata.xml, and starts `AudioPluginView`
                    // instantiation step. The factory might then continue to View instantiation, or
                    // invokes native `create()` extension function to let native code create View
                    // and set as content to `AudioPluginView`.
                    

                    withGuiExtension<int32_t>(plugin, 0, [=](aap_gui_extension_t *ext,
                                                             AndroidAudioPluginExtensionTarget target) {
                        auto guiInstanceId = ext->create(target, pluginId, instanceId);
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