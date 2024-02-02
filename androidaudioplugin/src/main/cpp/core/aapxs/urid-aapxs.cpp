
#include "aap/core/aapxs/urid-aapxs.h"
#include "../AAPJniFacade.h"

void aap::xs::AAPXSDefinition_Urid::aapxs_urid_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    auto ext = (aap_urid_extension_t*) plugin->get_extension(plugin, AAP_URID_EXTENSION_URI);
    if (!ext)
        return; // FIXME: should there be any global error handling?
    switch (request->opcode) {
        case OPCODE_MAP:
            uint8_t urid = *(uint8_t*) request->serialization->data;
            auto len = *(int32_t *) ((uint8_t*) request->serialization->data + 1);
            if (len >= AAP_MAX_EXTENSION_URI_SIZE)
                AAP_ASSERT_FALSE;
            else {
                char uri[AAP_MAX_EXTENSION_URI_SIZE];
                strncpy(uri, (const char *) (1 + (int32_t *) request->serialization->data), len);
                ext->map(ext, plugin, urid, uri);
            }
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
    }
}

void aap::xs::AAPXSDefinition_Urid::aapxs_urid_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    throw std::runtime_error("There is no URID host extension");
}

void aap::xs::AAPXSDefinition_Urid::aapxs_urid_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Urid::aapxs_urid_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

AAPXSExtensionClientProxy
aap::xs::AAPXSDefinition_Urid::aapxs_urid_get_plugin_proxy(struct AAPXSDefinition *feature,
                                                           AAPXSInitiatorInstance *aapxsInstance,
                                                           AAPXSSerializationContext *serialization) {
    auto client = (AAPXSDefinition_Urid*) feature->aapxs_context;
    if (!client->typed_client)
        client->typed_client = std::make_unique<UridClientAAPXS>(aapxsInstance, serialization);
    *client->typed_client = UridClientAAPXS(aapxsInstance, serialization);
    client->client_proxy = AAPXSExtensionClientProxy{client->typed_client.get(), aapxs_urid_as_plugin_extension};
    return client->client_proxy;
}

void aap::xs::UridClientAAPXS::map(uint8_t urid, const char *uri) {
    *(uint8_t*) serialization->data = urid;
    size_t len = strlen(uri);
    *(uint32_t*) ((uint8_t*) serialization->data + 1) = len;
    memcpy((uint8_t*) serialization->data + 1 + sizeof(int32_t), uri, len);
    callVoidFunctionSynchronously(OPCODE_MAP);
}
