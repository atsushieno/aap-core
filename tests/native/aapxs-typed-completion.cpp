#include <cstdio>
#include <memory>
#include <stdexcept>
#include "aap/core/aapxs/typed-aapxs.h"
using namespace aap;
int main() {
    uint32_t id = 0;
    AAPXSRequestContext pending{};
    struct Context { uint32_t* id; AAPXSRequestContext* pending; } context{&id, &pending};
    int value = 42;
    AAPXSSerializationContext data{&value, 0, 4};
    AAPXSInitiatorInstance initiator{&context, nullptr, &data, 1,
        [](auto* self) { return ++*static_cast<Context*>(self->aapxs_context)->id; },
        [](auto* self, auto* request) {
            *static_cast<Context*>(self->aapxs_context)->pending = *request;
            return true;
        }};
    auto client = std::make_unique<xs::TypedAAPXS>("urn:aap:typed-completion-test", &initiator, &data);
    int callbacks = 0;
    client->callFunctionAsync(1, &value, 4, [&](const std::string& error, auto* reply, void*) {
        if (!error.empty() || *static_cast<int*>(reply->data) != 42)
            throw std::runtime_error("self-destroying callback reply");
        client.reset();
        ++callbacks;
    }, 4);
    pending.callback(pending.callback_user_data, nullptr);
    if (callbacks != 1 || client)
        throw std::runtime_error("callback completion ownership");
    puts("AAPXS typed-completion regression tests passed");
}
