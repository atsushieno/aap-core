#pragma once

#include "aap/core/aapxs/standard-extensions.h"
#include "aap/core/plugin-information.h"

namespace aap::internal {
// Value-only public getters keep their historical defaults. Scanning must retain transport
// errors, otherwise a failed read can silently replace the layout with zero-filled metadata.
class ParameterLayoutReader {
    xs::StandardExtensions* local{};
    xs::TypedAAPXS* transport{};

    template<typename T>
    Result<T> read(int32_t opcode, const void* payload, size_t size) {
        auto result = transport->callAndWait<Result<T>>(opcode, payload, size,
            [](AAPXSSerializationContext* ctx) -> Result<T> {
                if (!ctx || !ctx->data || ctx->data_size < sizeof(T))
                    return {T{}, "short parameter reply"};
                T value{};
                memcpy(&value, ctx->data, sizeof(value));
                return {value, ""};
            }, sizeof(T));
        return result.isOk() ? std::move(result.value) : Result<T>{T{}, result.error};
    }
public:
    explicit ParameterLayoutReader(xs::StandardExtensions& extensions) : local(&extensions) {
        if (auto* client = dynamic_cast<xs::ClientStandardExtensions*>(&extensions)) {
            auto* proxy = client->asParametersExtension();
            if (proxy)
                transport = static_cast<xs::ParametersClientAAPXS*>(proxy->aapxs_context);
        }
    }
    explicit ParameterLayoutReader(xs::TypedAAPXS& client) : transport(&client) {}

    Result<int32_t> count() {
        return transport ? read<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT, nullptr, 0)
                         : Result<int32_t>{local->getParameterCount(), ""};
    }
    Result<aap_parameter_info_t> parameter(int32_t index) {
        return transport ? read<aap_parameter_info_t>(OPCODE_PARAMETERS_GET_PARAMETER, &index, sizeof(index))
                         : Result<aap_parameter_info_t>{local->getParameter(index), ""};
    }
    Result<int32_t> enumerationCount(int32_t id) {
        return transport ? read<int32_t>(OPCODE_PARAMETERS_GET_ENUMERATION_COUNT, &id, sizeof(id))
                         : Result<int32_t>{local->getEnumerationCount(id), ""};
    }
    Result<aap_parameter_enum_t> enumeration(int32_t id, int32_t index) {
        int32_t payload[]{id, index};
        return transport ? read<aap_parameter_enum_t>(OPCODE_PARAMETERS_GET_ENUMERATION, payload, sizeof(payload))
                         : Result<aap_parameter_enum_t>{local->getEnumeration(id, index), ""};
    }
};

// Build the whole replacement before publishing it. An error never produces a partial layout.
inline Result<std::vector<ParameterInformation>> readParameterLayout(ParameterLayoutReader& reader) {
    constexpr int32_t maxItems = 16384;
    auto count = reader.count();
    if (!count.isOk())
        return {{}, count.error};
    if (count.value < 0)
        return {{}, "parameters extension unavailable"};
    if (count.value > maxItems)
        return {{}, "invalid parameter count"};
    std::vector<ParameterInformation> layout;
    layout.reserve(count.value);
    for (int32_t i = 0; i < count.value; ++i) {
        auto parameter = reader.parameter(i);
        if (!parameter.isOk())
            return {{}, parameter.error};
        const auto& para = parameter.value;
        auto enums = reader.enumerationCount(para.stable_id);
        if (!enums.isOk())
            return {{}, enums.error};
        if (para.stable_id < 0 || enums.value < 0 || enums.value > maxItems)
            return {{}, "invalid parameter ID or enumeration count"};
        ParameterInformation info{para.stable_id,
            std::string(para.display_name, strnlen(para.display_name, AAP_MAX_PARAMETER_NAME_CHARS)),
            para.min_value, para.max_value, para.default_value};
        for (int32_t e = 0; e < enums.value; ++e) {
            auto enumeration = reader.enumeration(para.stable_id, e);
            if (!enumeration.isOk())
                return {{}, enumeration.error};
            const auto& value = enumeration.value;
            ParameterInformation::Enumeration definition{e, value.value,
                std::string(value.name, strnlen(value.name, AAP_MAX_PARAMETER_ENUM_NAME))};
            info.addEnumeration(definition);
        }
        layout.emplace_back(std::move(info));
    }
    return {std::move(layout), ""};
}
}
