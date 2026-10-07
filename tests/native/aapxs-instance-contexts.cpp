#include <atomic>
#include <cstdio>
#include <future>
#include <stdexcept>
#include <thread>
#include "aap/core/aapxs/standard-extensions.h"
#include "aap/core/aapxs/aapxs-lifecycle.h"
#include "aap/core/realtime.h"
namespace aap {
AAPJniFacade* AAPJniFacade::getInstance() { static AAPJniFacade value; return &value; }
int32_t AAPJniFacade::getMidiSettingsFromLocalConfig(std::string) { return 0; }
}
using namespace aap::xs;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
struct Context { bool host; const void* instance; };
struct Counts { int init{0}, release{0}; bool fail{false}; };
AAPXSDefinition custom(Counts& counts) {
    AAPXSDefinition d{}; d.uri = "urn:test:lifecycle"; d.data_capacity = 16; d.aapxs_context = &counts;
    d.initialize_initiator_instance = [](auto* d, auto* instance, bool host) {
        ++static_cast<Counts*>(d->aapxs_context)->init;
        instance->aapxs_context = new Context{host, instance}; return true;
    };
    d.initialize_recipient_instance = [](auto* d, auto* instance, bool host) {
        auto& counts = *static_cast<Counts*>(d->aapxs_context); ++counts.init;
        instance->aapxs_context = new Context{host, instance}; return !counts.fail;
    };
    d.release_initiator_instance = [](auto* d, auto* instance, bool host) {
        auto* context = static_cast<Context*>(instance->aapxs_context);
        check(context->host == host && context->instance == instance && instance->serialization, "initiator role/address/buffer retained during release");
        ++static_cast<Counts*>(d->aapxs_context)->release; delete context;
    };
    d.release_recipient_instance = [](auto* d, auto* instance, bool host) {
        auto* context = static_cast<Context*>(instance->aapxs_context);
        check(context->host == host && context->instance == instance && instance->serialization, "recipient role/address/buffer retained during release");
        ++static_cast<Counts*>(d->aapxs_context)->release; delete context;
    };
    return d;
}
void rolesAndFailure() {
    Counts counts;
    AAPXSDefinitionRegistry registry(std::make_unique<UridMapping>(), {custom(counts)});
    auto allocate = [](const char*, auto* data) { data->data = nullptr; return true; };
    {
        AAPXSClientDispatcher client(&registry);
        check(client.setupInstances(reinterpret_cast<void*>(1), allocate, nullptr, nullptr, nullptr), "client setup");
        AAPXSServiceDispatcher service(&registry);
        check(service.setupInstances(reinterpret_cast<void*>(2), [](const char*, auto*) {}, nullptr, nullptr, nullptr), "service setup");
        check(counts.init == 4 && counts.release == 0, "both roles initialized eagerly on both sides");
    }
    check(counts.release == 4, "all four contexts released once");
    counts.fail = true;
    { AAPXSClientDispatcher client(&registry);
      check(!client.setupInstances(nullptr, allocate, nullptr, nullptr, nullptr), "reject failed client initialization"); }
    { AAPXSServiceDispatcher service(&registry);
      check(!service.setupInstances(nullptr, [](const char*, auto*) {}, nullptr, nullptr, nullptr), "reject failed service initialization"); }
    check(counts.init == 8 && counts.release == 8, "partial setup cleans contexts exactly once");
}
void standardOwnership() {
    auto* registry = AAPXSDefinitionRegistry::getStandardExtensions();
    AAPXSClientDispatcher first(registry), second(registry);
    auto allocate = [](const char*, auto*) { return true; };
    check(first.setupInstances(reinterpret_cast<void*>(1), allocate, nullptr, nullptr, nullptr), "first standard instance");
    check(second.setupInstances(reinterpret_cast<void*>(2), allocate, nullptr, nullptr, nullptr), "second standard instance");
    ClientStandardExtensions hosting; hosting.initialize(&first);
    auto* p = first.getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI);
    auto* q = second.getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI);
    check(p->aapxs_context && q->aapxs_context && p->aapxs_context != q->aapxs_context, "clients owned per instance");
    check(hosting.asParametersExtension()->aapxs_context == p->aapxs_context, "hosting borrows canonical parameter client");
    for (const char* uri : {AAP_PARAMETERS_EXTENSION_URI, AAP_PRESETS_EXTENSION_URI, AAP_STATE_EXTENSION_URI, AAP_MIDI_EXTENSION_URI, AAP_URID_EXTENSION_URI}) {
        auto* d = first.getDefinitionByUri(uri); auto* i = first.getPluginAAPXSByUri(uri);
        auto a = d->get_plugin_extension_proxy(d, i, i->serialization);
        auto b = d->get_plugin_extension_proxy(d, i, i->serialization);
        check(a.aapxs_context == i->aapxs_context && a.aapxs_context == b.aapxs_context, "proxy lookup reuses extension-owned client");
    }
    AAPXSServiceDispatcher service(registry);
    check(service.setupInstances(reinterpret_cast<void*>(3), [](const char*, auto*) {}, nullptr, nullptr, nullptr), "standard service setup");
    for (const char* uri : {AAP_PARAMETERS_EXTENSION_URI, AAP_PRESETS_EXTENSION_URI}) {
        auto* d = service.getDefinitionByUri(uri); auto* i = service.getHostAAPXSByUri(uri);
        check(i->aapxs_context, "notification clients created before processing");
        aap::RealtimeScope guard;
        auto a = d->get_host_extension_proxy(d, i, i->serialization);
        check(a.aapxs_context == i->aapxs_context, "processing lookup has no lazy creation");
    }
}
void publicAbortService() {
    uint8_t bytes[16]{}; AAPXSSerializationContext data{bytes, 0, sizeof(bytes)};
    auto registry = std::make_shared<AsyncAbortRegistry>();
    auto instance = std::make_unique<AAPXSInitiatorInstance>();
    instance->host_context = reinterpret_cast<void*>(1); // must never be interpreted as PluginInstance
    registry->attach(*instance);
    auto client = std::make_unique<TypedAAPXS>("urn:test:foreign-framework", instance.get(), &data);
    instance.reset(); registry.reset(); client.reset(); // unregister after framework/initiator lifetime
    registry = std::make_shared<AsyncAbortRegistry>();
    AAPXSInitiatorInstance i{}; registry->attach(i);
    std::promise<void> entered, finish;
    auto finished = finish.get_future().share();
    struct Handler { std::promise<void>* entered; std::shared_future<void> finish; } handler{&entered, finished};
    auto registration = i.register_abort_handler(&i, &handler, [](void* p, const char*) {
        auto& h = *static_cast<Handler*>(p); h.entered->set_value(); h.finish.wait();
    });
    auto abort = std::async(std::launch::async, [&] { registry->abort("closed"); });
    entered.get_future().wait();
    auto unregister = std::async(std::launch::async, [&] { i.unregister_abort_handler(registration); });
    check(unregister.wait_for(std::chrono::milliseconds(20)) != std::future_status::ready, "unregister excludes in-flight delivery");
    finish.set_value(); abort.get(); unregister.get(); registry->abort("again");
    // Reentrant client destruction during cancellation with multiple pending calls.
    std::vector<AAPXSRequestContext> requests;
    i.aapxs_context = &requests;
    i.get_new_request_id = [](auto*) { static uint32_t next{}; return ++next; };
    i.send_aapxs_request = [](auto* self, auto* request) { static_cast<std::vector<AAPXSRequestContext>*>(self->aapxs_context)->push_back(*request); return true; };
    client = std::make_unique<TypedAAPXS>("urn:test:self-release", &i, &data);
    int calls = 0;
    for (int n = 0; n < 2; ++n)
        client->callFunctionAsync(1, nullptr, 0, [&](const std::string&, auto*, void*) { ++calls; client.reset(); });
    registry->abort("shutdown");
    check(!client && calls == 2, "reentrant destruction completes all calls without accessing a dead client");
    requests.back().callback(requests.back().callback_user_data, nullptr); // release detached late context
}
int main() { rolesAndFailure(); standardOwnership(); publicAbortService(); puts("PASS: eager contexts, role-aware cleanup, extension ownership and public cancellation lifecycle"); }
