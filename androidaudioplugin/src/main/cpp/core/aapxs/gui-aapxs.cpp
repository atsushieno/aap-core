
#include <cstring>
#include "aap/core/aapxs/gui-aapxs.h"

// ---------------------------------------------------------------------------
// Service side: plugin receives a request from the host and executes it.
// ---------------------------------------------------------------------------

void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_gui_extension_t*) plugin->get_extension(plugin, AAP_GUI_EXTENSION_URI);
    if (!ext)
        return;

    switch (request->opcode) {
        case OPCODE_CREATE_GUI: {
            const char* pluginId = (const char*) request->serialization->data;
            int32_t instanceId = *((int32_t*)((uint8_t*) request->serialization->data + AAP_MAX_PLUGIN_ID_SIZE));
            void* audioPluginView = *((void**)((uint8_t*) request->serialization->data + AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t)));
            aap_gui_instance_id result = AAP_GUI_ERROR_NO_CREATE_DEFINED;
            if (ext->create)
                result = ext->create(ext, plugin, pluginId, instanceId, audioPluginView);
            *((int32_t*) request->serialization->data) = result;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_SHOW_GUI: {
            int32_t guiInstanceId = *((int32_t*) request->serialization->data);
            int32_t result = ext->show ? ext->show(ext, plugin, guiInstanceId) : AAP_GUI_ERROR_NO_SHOW_DEFINED;
            *((int32_t*) request->serialization->data) = result;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_HIDE_GUI: {
            int32_t guiInstanceId = *((int32_t*) request->serialization->data);
            int32_t result = AAP_GUI_ERROR_NO_HIDE_DEFINED;
            if (ext->hide) {
                ext->hide(ext, plugin, guiInstanceId);
                result = AAP_GUI_RESULT_OK;
            }
            *((int32_t*) request->serialization->data) = result;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_RESIZE_GUI: {
            auto data = (int32_t*) request->serialization->data;
            int32_t guiInstanceId = data[0];
            int32_t width  = data[1];
            int32_t height = data[2];
            int32_t result = ext->resize ? ext->resize(ext, plugin, guiInstanceId, width, height) : AAP_GUI_ERROR_NO_RESIZE_DEFINED;
            *((int32_t*) request->serialization->data) = result;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case OPCODE_DESTROY_GUI: {
            int32_t guiInstanceId = *((int32_t*) request->serialization->data);
            int32_t result = AAP_GUI_ERROR_NO_DESTROY_DEFINED;
            if (ext->destroy) {
                ext->destroy(ext, plugin, guiInstanceId);
                result = AAP_GUI_RESULT_OK;
            }
            *((int32_t*) request->serialization->data) = result;
            request->serialization->data_size = sizeof(int32_t);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        default:
            break;
    }
}

// No host extension opcodes are defined for GUI.
void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    // nothing
}

// ---------------------------------------------------------------------------
// Client (host) side: handle the reply from the plugin service.
// The callback fulfils the std::promise set up in TypedAAPXS.
// ---------------------------------------------------------------------------

void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Gui::aapxs_gui_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

// ---------------------------------------------------------------------------
// GuiClientAAPXS: host-side methods that serialize arguments and invoke
// the synchronous AAPXS call, then read back the result.
// ---------------------------------------------------------------------------

aap_gui_instance_id aap::xs::GuiClientAAPXS::createGui(std::string pluginId, int32_t instanceId,
                                                        void* audioPluginView) {
    memset(serialization->data, 0, AAP_MAX_PLUGIN_ID_SIZE);
    strncpy((char*) serialization->data, pluginId.c_str(), AAP_MAX_PLUGIN_ID_SIZE - 1);
    *((int32_t*)((uint8_t*) serialization->data + AAP_MAX_PLUGIN_ID_SIZE)) = instanceId;
    *((void**)((uint8_t*) serialization->data + AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t))) = audioPluginView;
    serialization->data_size = AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t) + sizeof(void*);
    return callTypedFunctionSynchronously<int32_t>(OPCODE_CREATE_GUI);
}

int32_t aap::xs::GuiClientAAPXS::showGui(aap_gui_instance_id guiInstanceId) {
    *((int32_t*) serialization->data) = guiInstanceId;
    serialization->data_size = sizeof(int32_t);
    return callTypedFunctionSynchronously<int32_t>(OPCODE_SHOW_GUI);
}

int32_t aap::xs::GuiClientAAPXS::hideGui(aap_gui_instance_id guiInstanceId) {
    *((int32_t*) serialization->data) = guiInstanceId;
    serialization->data_size = sizeof(int32_t);
    return callTypedFunctionSynchronously<int32_t>(OPCODE_HIDE_GUI);
}

int32_t aap::xs::GuiClientAAPXS::resizeGui(aap_gui_instance_id guiInstanceId, int32_t width, int32_t height) {
    auto data = (int32_t*) serialization->data;
    data[0] = guiInstanceId;
    data[1] = width;
    data[2] = height;
    serialization->data_size = sizeof(int32_t) * 3;
    return callTypedFunctionSynchronously<int32_t>(OPCODE_RESIZE_GUI);
}

int32_t aap::xs::GuiClientAAPXS::destroyGui(aap_gui_instance_id guiInstanceId) {
    *((int32_t*) serialization->data) = guiInstanceId;
    serialization->data_size = sizeof(int32_t);
    return callTypedFunctionSynchronously<int32_t>(OPCODE_DESTROY_GUI);
}
