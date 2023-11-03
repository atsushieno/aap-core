
#include "aap/core/aapxs/gui-aapxs.h"

void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    // FIXME: implement
    throw std::runtime_error("implement");
}

void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}

void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}

void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}

aap_gui_instance_id aap::xs::GuiClientAAPXS::createGui(std::string pluginId, int32_t instanceId,
                                                       void *audioPluginView) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}

int32_t aap::xs::GuiClientAAPXS::showGui(aap_gui_instance_id guiInstanceId) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}

int32_t aap::xs::GuiClientAAPXS::hideGui(aap_gui_instance_id guiInstanceId) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}

int32_t aap::xs::GuiClientAAPXS::resizeGui(aap_gui_instance_id guiInstanceId, int32_t width,
                                           int32_t height) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}

int32_t aap::xs::GuiClientAAPXS::destroyGui(aap_gui_instance_id guiInstanceId) {
    // FIXME: implement, but we might rather just invoke JniFacade as we did in StandardExtensions v1
    throw std::runtime_error("implement");
}
