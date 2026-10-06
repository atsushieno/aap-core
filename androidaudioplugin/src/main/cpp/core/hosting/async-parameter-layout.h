#pragma once

#include "parameter-layout-reader.h"
#include <atomic>
#include <mutex>

namespace aap::internal {
// Only one request is outstanding. Reply callbacks publish the next step; the
// extension worker runs it after leaving its Binder-only completion scope.
// RT-unsafe metadata reads retain their Binder route. For legacy peers only, a
// count request between reads serves as a drain barrier: legacy services
// also queue MIDI replies to Binder reads, and must drain them before another
// read. Inactive instances complete both requests through Binder without audio.
class AsyncParameterLayout : public std::enable_shared_from_this<AsyncParameterLayout> {
    xs::TypedAAPXS& transport;
    const bool legacy_drain;
    std::function<void()> wake;
    std::mutex mailbox_mutex;
    std::function<void(AsyncParameterLayout&)> next;
    int32_t count{0}, index{0}, enum_count{0}, enum_index{0};
    aap_parameter_info_t parameter{};
    std::vector<ParameterInformation> layout;
    bool done{false};
    std::string error;
    static constexpr int32_t max_items = 16384;

    void fail(std::string message) { error = std::move(message); done = true; }

    template<typename T>
    void read(int32_t opcode, const void* payload, size_t size,
              std::function<void(AsyncParameterLayout&, const T&)> consume) {
        auto self = shared_from_this();
        transport.callFunctionAsync(opcode, payload, size,
            [self, opcode, consume](const std::string& error, AAPXSSerializationContext* data, void*) {
                Result<T> result{T{}, error};
                if (error.empty()) {
                    if (!data || !data->data || data->data_size < sizeof(T))
                        result.error = "short parameter reply";
                    else
                        memcpy(&result.value, data->data, sizeof(T));
                }
                {
                    const std::lock_guard<std::mutex> lock{self->mailbox_mutex};
                    self->next = [opcode, consume, result = std::move(result)](auto& scan) {
                        if (!result.isOk()) scan.fail(result.error);
                        else if (!scan.legacy_drain || opcode == OPCODE_PARAMETERS_GET_PARAMETER_COUNT) consume(scan, result.value);
                        else scan.template read<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT, nullptr, 0,
                            [consume, result](auto& scan, int32_t) { consume(scan, result.value); });
                    };
                }
                self->wake();
            }, sizeof(T));
    }
    void readParameter() {
        if (index == count) { done = true; return; }
        read<aap_parameter_info_t>(OPCODE_PARAMETERS_GET_PARAMETER, &index, sizeof(index),
            [](auto& self, const auto& value) {
                if (value.stable_id < 0) { self.fail("invalid parameter ID"); return; }
                self.parameter = value;
                int32_t id = value.stable_id;
                self.template read<int32_t>(OPCODE_PARAMETERS_GET_ENUMERATION_COUNT, &id,
                    sizeof(id), [](auto& self, int32_t count) {
                        if (count < 0 || count > max_items) { self.fail("invalid enumeration count"); return; }
                        const auto& p = self.parameter;
                        self.layout.emplace_back(p.stable_id,
                            std::string(p.display_name, strnlen(p.display_name, AAP_MAX_PARAMETER_NAME_CHARS)),
                            p.min_value, p.max_value, p.default_value);
                        self.enum_count = count; self.enum_index = 0;
                        self.readEnumeration();
                    });
            });
    }
    void readEnumeration() {
        if (enum_index == enum_count) { ++index; readParameter(); return; }
        int32_t payload[]{parameter.stable_id, enum_index};
        read<aap_parameter_enum_t>(OPCODE_PARAMETERS_GET_ENUMERATION, payload, sizeof(payload),
            [](auto& self, const auto& value) {
                ParameterInformation::Enumeration enumeration{self.enum_index, value.value,
                    std::string(value.name, strnlen(value.name, AAP_MAX_PARAMETER_ENUM_NAME))};
                self.layout.back().addEnumeration(enumeration);
                ++self.enum_index;
                self.readEnumeration();
            });
    }
public:
    AsyncParameterLayout(xs::TypedAAPXS& transport, bool legacyDrain, std::function<void()> wake)
        : transport(transport), legacy_drain(legacyDrain), wake(std::move(wake)) {}
    void start() {
        read<int32_t>(OPCODE_PARAMETERS_GET_PARAMETER_COUNT, nullptr, 0, [](auto& self, int32_t count) {
            if (count < 0) { self.fail("parameters extension unavailable"); return; }
            if (count > max_items) { self.fail("invalid parameter count"); return; }
            self.count = count; self.layout.reserve(count); self.readParameter();
        });
    }
    // Supersede at a reply boundary. A request already handed to the transport
    // retains its buffer/callback until delivery, rather than queuing abandoned
    // scans behind it when notifications arrive repeatedly.
    bool canSupersede() {
        const std::lock_guard<std::mutex> lock{mailbox_mutex};
        return done || static_cast<bool>(next);
    }
    bool poll(const std::atomic<bool>* superseded = nullptr) {
        // Binder replies may be immediate. Bound work per dispatch and let the
        // eventfd notification schedule remaining steps without recursion.
        for (unsigned i = 0; i < 64 && !done; ++i) {
            if (superseded && superseded->load(std::memory_order_acquire)) break;
            std::function<void(AsyncParameterLayout&)> step;
            {
                const std::lock_guard<std::mutex> lock{mailbox_mutex};
                step = std::move(next);
            }
            if (!step) break;
            step(*this);
        }
        return done;
    }
    Result<std::vector<ParameterInformation>> takeResult() { return {std::move(layout), std::move(error)}; }
};
}
