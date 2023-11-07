#ifndef AAP_CORE_GUI_AAPXS_H
#define AAP_CORE_GUI_AAPXS_H

#include <functional>
#include <future>
#include "aap/aapxs.h"
#include "../../ext/gui.h"
#include "typed-aapxs.h"

// plugin extension opcodes
const int32_t OPCODE_CREATE_GUI = 1;
const int32_t OPCODE_SHOW_GUI = 2;
const int32_t OPCODE_HIDE_GUI = 3;
const int32_t OPCODE_RESIZE_GUI = 4;
const int32_t OPCODE_DESTROY_GUI = 5;

// host extension opcodes
// ... nothing?

const int32_t GUI_SHARED_MEMORY_SIZE = AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t) + sizeof(void*);

namespace aap::xs {
    class AAPXSDefinition_Gui : public AAPXSDefinitionWrapper {

        static void aapxs_gui_process_incoming_plugin_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_gui_process_incoming_host_aapxs_request(
                struct AAPXSDefinition* feature,
                AAPXSRecipientInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);
        static void aapxs_gui_process_incoming_plugin_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPlugin* plugin,
                AAPXSRequestContext* request);
        static void aapxs_gui_process_incoming_host_aapxs_reply(
                struct AAPXSDefinition* feature,
                AAPXSInitiatorInstance* aapxsInstance,
                AndroidAudioPluginHost* host,
                AAPXSRequestContext* request);

        AAPXSDefinition aapxs_gui{this,
                                  AAP_GUI_EXTENSION_URI,
                                  GUI_SHARED_MEMORY_SIZE,
                                  aapxs_gui_process_incoming_plugin_aapxs_request,
                                  aapxs_gui_process_incoming_host_aapxs_request,
                                  aapxs_gui_process_incoming_plugin_aapxs_reply,
                                  aapxs_gui_process_incoming_host_aapxs_reply,
                                  nullptr, // not sure if it is of any use
                                  nullptr // not sure if it is of any use
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_gui;
        }
    };

    class GuiClientAAPXS : public TypedAAPXS {
    public:
        GuiClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_GUI_EXTENSION_URI, initiatorInstance, serialization) {
        }

        aap_gui_instance_id createGui(std::string pluginId, int32_t instanceId, void* audioPluginView);
        int32_t showGui(aap_gui_instance_id guiInstanceId);
        int32_t hideGui(aap_gui_instance_id guiInstanceId);
        int32_t resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height);
        int32_t destroyGui(aap_gui_instance_id guiInstanceId);
    };

    class GuiServiceAAPXS : public TypedAAPXS {
    public:
        GuiServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAP_GUI_EXTENSION_URI, initiatorInstance, serialization) {
        }

        // nothing?
    };
}


#endif //AAP_CORE_GUI_AAPXS_H
