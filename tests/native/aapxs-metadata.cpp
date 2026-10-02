#include <cstdio>
#include <stdexcept>
#include "parameter-layout-reader.h"

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
int main() {
    parameterScans();
    puts("AAPXS parameter metadata regression tests passed");
}
