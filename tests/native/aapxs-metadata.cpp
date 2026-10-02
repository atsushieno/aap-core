#include <cstdio>
#include <stdexcept>
#include "parameter-layout-reader.h"
#include "midi-policy-payload.h"

namespace {
std::string queriedPlugin;
}
aap::AAPJniFacade* aap::AAPJniFacade::getInstance() {
    static AAPJniFacade facade;
    return &facade;
}
int32_t aap::AAPJniFacade::getMidiSettingsFromLocalConfig(std::string pluginId) {
    queriedPlugin = pluginId;
    return AAP_PARAMETERS_MAPPING_POLICY_CC;
}
using namespace aap;
using namespace aap::internal;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct ParameterTransport {
    uint32_t id{};
    int reads{};
    int failAt{-1};
    bool shortReply{};
    int32_t count{2};
    int32_t enumCount{2};
    AAPXSInitiatorInstance initiator{this, nullptr, nullptr, 1,
        [](auto* self) { return ++static_cast<ParameterTransport*>(self->aapxs_context)->id; },
        [](auto* self, auto* request) {
            auto* test = static_cast<ParameterTransport*>(self->aapxs_context);
            auto failure = test->reads++ == test->failAt;
            if (failure && !test->shortReply) {
                request->error_callback(request->callback_user_data, nullptr, "injected read failure");
                return true;
            }
            auto* data = request->serialization;
            int32_t index{};
            if (data->data_size >= sizeof(index))
                memcpy(&index, data->data, sizeof(index));
            switch (request->opcode) {
                case OPCODE_PARAMETERS_GET_PARAMETER_COUNT:
                    memcpy(data->data, &test->count, sizeof(test->count));
                    data->data_size = sizeof(test->count);
                    break;
                case OPCODE_PARAMETERS_GET_PARAMETER: {
                    aap_parameter_info_t parameter{};
                    parameter.stable_id = index == 0 ? 17 : 42;
                    memset(parameter.display_name, 'p', sizeof(parameter.display_name));
                    parameter.max_value = 100;
                    parameter.default_value = 30;
                    memcpy(data->data, &parameter, sizeof(parameter));
                    data->data_size = sizeof(parameter);
                    break;
                }
                case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT: {
                    int32_t count = index == 17 ? test->enumCount : 0;
                    memcpy(data->data, &count, sizeof(count));
                    data->data_size = sizeof(count);
                    break;
                }
                case OPCODE_PARAMETERS_GET_ENUMERATION: {
                    aap_parameter_enum_t enumeration{};
                    enumeration.value = 23;
                    memset(enumeration.name, 'e', sizeof(enumeration.name));
                    memcpy(data->data, &enumeration, sizeof(enumeration));
                    data->data_size = sizeof(enumeration);
                    break;
                }
            }
            if (failure)
                data->data_size = 0;
            request->callback(request->callback_user_data, nullptr);
            return true;
        }};
    char buffer[PARAMETERS_SHARED_MEMORY_SIZE]{};
    AAPXSSerializationContext serialization{buffer, 0, sizeof(buffer)};
    xs::TypedAAPXS client{AAP_PARAMETERS_EXTENSION_URI, &initiator, &serialization};
};
void parameterScans() {
    ParameterTransport fixture;
    ParameterLayoutReader reader{fixture.client};
    auto success = readParameterLayout(reader);
    check(success.isOk() && success.value.size() == 2, "successful scan");
    check(success.value[0].getId() == 17 && success.value[1].getId() == 42, "stable IDs retained");
    check(success.value[0].getEnumCount() == 2 && success.value[1].getEnumCount() == 0, "enumerations retained");
    check(strlen(success.value[0].getName()) == AAP_MAX_PARAMETER_NAME_CHARS, "unterminated name bounded");
    int totalReads = fixture.reads;
    // Fail even the final read, after a full first parameter has already been scanned.
    for (bool shortReply : {false, true}) {
        fixture.shortReply = shortReply;
        for (int failure = 0; failure < totalReads; ++failure) {
            fixture.reads = 0;
            fixture.failAt = failure;
            auto result = readParameterLayout(reader);
            check(!result.isOk() && result.value.empty(), "failed reads cannot publish partial metadata");
            check(fixture.reads == failure + 1, "scan stops at first failure");
        }
    }
    fixture.failAt = -1;
    fixture.count = 16385;
    fixture.reads = 0;
    check(!readParameterLayout(reader).isOk() && fixture.reads == 1, "oversized count rejected before allocation");
    fixture.count = 2;
    fixture.enumCount = -1;
    check(!readParameterLayout(reader).isOk(), "negative enumeration count rejected");
    fixture.count = 0;
    auto empty = readParameterLayout(reader);
    check(empty.isOk() && empty.value.empty(), "zero parameters is a valid replacement");
    fixture.count = -1;
    check(!readParameterLayout(reader).isOk(), "unavailable extension preserves existing layout");
}
void midiIdentity() {
    char bytes[AAP_MAX_PLUGIN_ID_SIZE + sizeof(int32_t)]{};
    std::string id = "org.example.plugin";
    auto size = writeMidiPolicyPluginId(bytes, sizeof(bytes), id);
    AAPXSSerializationContext data{bytes, size, sizeof(bytes)};
    check(size == sizeof(int32_t) + id.size(), "existing wire record size");
    check(readMidiPolicyPluginId(data, "") == id, "identity wire roundtrip");
    data.data_size = 0;
    check(readMidiPolicyPluginId(data, "") == id, "legacy Binder unknown length");
    memset(bytes, 0xFF, sizeof(bytes));
    check(readMidiPolicyPluginId(data, id) == id, "old client stale data uses instance identity");
    check(readMidiPolicyPluginId(data, "").empty(), "negative length rejected");
    int32_t length = AAP_MAX_PLUGIN_ID_SIZE;
    memcpy(bytes, &length, sizeof(length));
    check(readMidiPolicyPluginId(data, "").empty(), "oversized length rejected");
    length = 5;
    memcpy(bytes, &length, sizeof(length));
    data.data_size = sizeof(int32_t) + 2;
    check(readMidiPolicyPluginId(data, "").empty(), "truncated request rejected");
    data.data_capacity = 2;
    check(readMidiPolicyPluginId(data, "").empty(), "short shared buffer rejected");
    std::string longest(AAP_MAX_PLUGIN_ID_SIZE - 1, 'x');
    check(writeMidiPolicyPluginId(bytes, sizeof(bytes), longest) > 0, "maximum valid plugin ID");
    longest += 'x';
    check(writeMidiPolicyPluginId(bytes, sizeof(bytes), longest) == 0, "invalid ID never sent");
}
void midiHandler() {
    xs::AAPXSDefinition_Midi wrapper;
    auto& definition = wrapper.asPublic();
    int replies = 0;
    AAPXSRecipientInstance recipient{&replies, nullptr, nullptr,
        [](auto* self, auto*) { ++*static_cast<int*>(self->aapxs_context); }};
    char bytes[MIDI_SHARED_MEMORY_SIZE]{};
    auto size = writeMidiPolicyPluginId(bytes, sizeof(bytes), "org.example.policy");
    AAPXSSerializationContext data{bytes, size, sizeof(bytes)};
    AAPXSRequestContext request{nullptr, nullptr, &data, 1, AAP_MIDI_EXTENSION_URI, 1, OPCODE_GET_MAPPING_POLICY};
    definition.process_incoming_plugin_aapxs_request(&definition, &recipient, nullptr, &request);
    int32_t policy{};
    memcpy(&policy, bytes, sizeof(policy));
    check(replies == 1 && data.data_size == sizeof(policy), "MIDI handler declares reply size");
    check(queriedPlugin == "org.example.policy" && policy == AAP_PARAMETERS_MAPPING_POLICY_CC, "MIDI handler queries intended preference key");
    queriedPlugin.clear();
    memset(bytes, 0xFF, sizeof(bytes));
    data.data_size = 0;
    definition.process_incoming_plugin_aapxs_request(&definition, &recipient, nullptr, &request);
    memcpy(&policy, bytes, sizeof(policy));
    check(replies == 2 && queriedPlugin.empty() && policy == 0, "malformed legacy request returns NONE without preference lookup");
    data.data_capacity = 2;
    definition.process_incoming_plugin_aapxs_request(&definition, &recipient, nullptr, &request);
    check(replies == 3 && data.data_size == 0, "short buffer receives an empty reply safely");
}
void midiClient() {
    struct Transport {
        uint32_t id{};
        int mode{};
        AAPXSInitiatorInstance initiator{this, nullptr, nullptr, 1,
            [](auto* self) { return ++static_cast<Transport*>(self->aapxs_context)->id; },
            [](auto* self, auto* request) {
                auto* test = static_cast<Transport*>(self->aapxs_context);
                auto* data = request->serialization;
                int32_t length = -1;
                memcpy(&length, data->data, sizeof(length));
                check(request->opcode == OPCODE_GET_MAPPING_POLICY && data->data_size == 4 && length == 0,
                      "client without metadata still sends a valid empty identity record");
                if (test->mode == 1) {
                    request->error_callback(request->callback_user_data, nullptr, "disconnected");
                    return true;
                }
                int32_t policy = AAP_PARAMETERS_MAPPING_POLICY_CC;
                memcpy(data->data, &policy, sizeof(policy));
                data->data_size = test->mode == 2 ? 2 : sizeof(policy);
                request->callback(request->callback_user_data, nullptr);
                return true;
            }};
    } fixture;
    char bytes[MIDI_SHARED_MEMORY_SIZE]{};
    AAPXSSerializationContext data{bytes, 0, sizeof(bytes)};
    xs::MidiClientAAPXS client{&fixture.initiator, &data};
    check(client.getMidiMappingPolicy() == AAP_PARAMETERS_MAPPING_POLICY_CC, "typed MIDI policy success");
    fixture.mode = 1;
    check(client.getMidiMappingPolicy() == AAP_PARAMETERS_MAPPING_POLICY_NONE, "typed MIDI policy transport failure");
    fixture.mode = 2;
    check(client.getMidiMappingPolicy() == AAP_PARAMETERS_MAPPING_POLICY_NONE, "typed MIDI short reply rejected");
}
int main() {
    parameterScans();
    midiIdentity();
    midiHandler();
    midiClient();
    puts("AAPXS metadata regression tests passed");
}
