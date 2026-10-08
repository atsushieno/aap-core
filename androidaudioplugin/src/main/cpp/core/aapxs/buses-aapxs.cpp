#include "aap/core/aapxs/buses-aapxs.h"
#include "aap/core/host/plugin-instance.h"
#include <stdexcept>

void aap::xs::AAPXSDefinition_Buses::aapxs_buses_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    (void) feature;
    (void) plugin;
    // The recipient's host context is always the LocalPluginInstance (see setupAAPXSInstances()).
    auto handler = static_cast<aap::LocalPluginInstance*>(aapxsInstance->host_context)->getBusesServiceHandler();
    auto* data = request->serialization;
    switch (request->opcode) {
        case OPCODE_BUSES_GET_LAYOUT: {
            if (!data || !data->data || data->data_capacity < sizeof(aap_buses_layout_snapshot_t)) {
                if (data)
                    data->data_size = 0;
                break;
            }
            aap_buses_layout_snapshot_t snapshot{};
            handler->getBusLayoutSnapshot(snapshot);
            memcpy(data->data, &snapshot, sizeof(snapshot));
            data->data_size = sizeof(snapshot);
            break;
        }
        case OPCODE_BUSES_COMMIT_BUFFER_LAYOUT: {
            int32_t accepted = 0;
            // Like other extensions, do not depend on the request size; it is not available on
            // every transport.
            if (data && data->data && data->data_capacity >= sizeof(aap_buffer_layout_t)) {
                aap_buffer_layout_t layout{};
                memcpy(&layout, data->data, sizeof(layout));
                accepted = handler->commitBufferLayout(layout) ? 1 : 0;
            }
            if (data && data->data && data->data_capacity >= sizeof(accepted)) {
                memcpy(data->data, &accepted, sizeof(accepted));
                data->data_size = sizeof(accepted);
            }
            break;
        }
        default:
            if (data)
                data->data_size = 0;
            break;
    }
    aapxsInstance->send_aapxs_reply(aapxsInstance, request);
}

void aap::xs::AAPXSDefinition_Buses::aapxs_buses_process_incoming_host_aapxs_request(
        struct AAPXSDefinition *feature, AAPXSRecipientInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    throw std::runtime_error("There is no buses host extension operation yet");
}

void aap::xs::AAPXSDefinition_Buses::aapxs_buses_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPlugin *plugin, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin);
}

void aap::xs::AAPXSDefinition_Buses::aapxs_buses_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition *feature, AAPXSInitiatorInstance *aapxsInstance,
        AndroidAudioPluginHost *host, AAPXSRequestContext *request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host);
}

aap::Result<aap_buses_layout_snapshot_t> aap::xs::BusesClientAAPXS::getLayout() {
    if (aap::RealtimeScope::isActive()) return {{}, "RT caller"};
    std::string error{};
    auto result = callAndWait<aap_buses_layout_snapshot_t>(OPCODE_BUSES_GET_LAYOUT, nullptr, 0,
        [&error](AAPXSSerializationContext* ctx) {
            aap_buses_layout_snapshot_t snapshot{};
            if (!ctx || !ctx->data || ctx->data_size < sizeof(snapshot) || ctx->data_capacity < sizeof(snapshot)) {
                error = "short buses layout reply";
                return snapshot;
            }
            memcpy(&snapshot, ctx->data, sizeof(snapshot));
            if (snapshot.count < 0 || snapshot.count > AAP_MAX_BUSES) {
                error = "invalid bus count";
                snapshot.count = 0;
            }
            return snapshot;
        }, sizeof(aap_buses_layout_snapshot_t));
    if (!result.isOk())
        return result;
    if (!error.empty())
        return {{}, error};
    return result;
}

aap::Result<bool> aap::xs::BusesClientAAPXS::commitBufferLayout(const aap_buffer_layout_t& layout) {
    if (aap::RealtimeScope::isActive()) return {false, "RT caller"};
    auto result = callAndWait<int32_t>(OPCODE_BUSES_COMMIT_BUFFER_LAYOUT, &layout, sizeof(layout),
        [](AAPXSSerializationContext* ctx) {
            int32_t accepted = 0;
            if (ctx && ctx->data && ctx->data_size >= sizeof(accepted) && ctx->data_capacity >= sizeof(accepted))
                memcpy(&accepted, ctx->data, sizeof(accepted));
            return accepted;
        }, sizeof(int32_t));
    if (!result.isOk())
        return {false, result.error};
    if (result.value != 1)
        return {false, "buffer layout rejected"};
    return {true, ""};
}
