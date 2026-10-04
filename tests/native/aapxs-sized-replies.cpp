#include <array>
#include <cstdio>
#include <stdexcept>
#include "aapxs-transport.h"
using namespace aap::internal;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    std::array<uint8_t, 64> sharedBytes{};
    AAPXSSerializationContext shared{sharedBytes.data(), 0, sharedBytes.size()};
    std::array<uint8_t, 8> targetBytes{};
    AAPXSSerializationContext target{targetBytes.data(), 0, targetBytes.size()};
    size_t actual = 4;
    bool legacy = false;
    int replies = 0, errors = 0;
    AAPXSRequestContext routed{};
    auto channel = std::make_shared<AAPXSBinderChannel>(&shared, [&](const auto& context) { routed = context; return true; },
        [&]() -> std::optional<size_t> { if (legacy) return std::nullopt; return actual; });
    struct Result { int* replies; int* errors; } result{&replies, &errors};
    AAPXSRequestContext request{[](void* context, void*) { ++*static_cast<Result*>(context)->replies; }, &result, &target, 1, "urn:aap:exact", 1, 1,
        [](void* context, void*, const char*) { ++*static_cast<Result*>(context)->errors; }};
    auto complete = [&] {
        targetBytes.fill(0x99); sharedBytes.fill(0x55); target.data_size = 0;
        check(channel->send(&request), "accepted");
        routed.callback(routed.callback_user_data, nullptr);
    };
    complete();
    check(replies == 1 && errors == 0 && target.data_size == 4 && targetBytes[3] == 0x55 && targetBytes[4] == 0x99, "copy only actual four bytes");
    actual = 1; complete();
    check(target.data_size == 1 && targetBytes[0] == 0x55 && targetBytes[1] == 0x99, "short reply length retained");
    actual = 0; complete(); check(target.data_size == 0 && targetBytes[0] == 0x99, "empty reply copies nothing");
    actual = 9; complete(); check(errors == 1 && target.data_size == 0 && targetBytes[0] == 0x99, "caller-capacity overflow returns error without copying");
    actual = 65; complete(); check(errors == 2, "shared-capacity overflow rejected");
    actual = UINT32_MAX; complete(); check(errors == 3, "missing reply sentinel rejected on both 32- and 64-bit targets");
    legacy = true; complete();
    check(target.data_size == 8 && targetBytes.back() == 0x55 && errors == 3, "legacy fallback retains former capacity-limited copy");
    puts("PASS: exact, short, empty and invalid Binder reply sizes with legacy fallback");
}
