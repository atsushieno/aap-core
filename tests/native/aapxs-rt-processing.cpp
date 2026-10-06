#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <future>
#include <new>
#include <pthread.h>
#include <stdexcept>
#include <sys/mman.h>
#include <thread>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include "aap/core/host/plugin-instance.h"
#include "aap/core/host/shared-memory-store.h"
#include "instance-realtime-state.h"
#include "aapxs-shared-transport.h"
#include "plugin-parameter-state.h"
#include "parameter-value-cache.h"

using namespace aap;
using namespace aap::internal;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void refuseRealtime(const char* operation) {
    if (RealtimeScope::isActive()) {
        write(STDERR_FILENO, operation, strlen(operation));
        __builtin_trap();
    }
}
void* operator new(size_t size) {
    refuseRealtime("C++ allocation");
    if (auto* data = malloc(size ? size : 1)) return data;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* data) noexcept { refuseRealtime("C++ reclamation"); free(data); }
void operator delete[](void* data) noexcept { ::operator delete(data); }
// Keep the dyld hook independent of framework/runtime initialization. Test-only
// scope hooks publish thread IDs into this fixed array for the lock interposer.
std::array<std::atomic<pthread_t>, 16> guardedProcessingThreads{};
extern "C" void aap_test_enter_processing() {
    for (auto& slot : guardedProcessingThreads) {
        pthread_t empty{};
        if (slot.compare_exchange_strong(empty, pthread_self())) return;
    }
    std::abort();
}
extern "C" void aap_test_leave_processing() {
    for (auto& slot : guardedProcessingThreads)
        if (slot.load() == pthread_self()) { slot.store(pthread_t{}); return; }
    std::abort();
}
extern "C" bool aap_test_is_processing() {
    for (auto& slot : guardedProcessingThreads)
        if (slot.load() == pthread_self()) return true;
    return false;
}
#ifndef __APPLE__
extern "C" int pthread_mutex_lock(pthread_mutex_t* mutex) {
    refuseRealtime("pthread mutex lock");
    static auto actual = reinterpret_cast<int(*)(pthread_mutex_t*)>(dlsym(RTLD_NEXT, "pthread_mutex_lock"));
    return actual(mutex);
}
extern "C" int pthread_mutex_trylock(pthread_mutex_t* mutex) {
    refuseRealtime("pthread mutex try-lock");
    static auto actual = reinterpret_cast<int(*)(pthread_mutex_t*)>(dlsym(RTLD_NEXT, "pthread_mutex_trylock"));
    return actual(mutex);
}
extern "C" int pthread_rwlock_rdlock(pthread_rwlock_t* lock) {
    refuseRealtime("pthread read lock");
    static auto actual = reinterpret_cast<int(*)(pthread_rwlock_t*)>(dlsym(RTLD_NEXT, "pthread_rwlock_rdlock"));
    return actual(lock);
}
extern "C" int pthread_rwlock_wrlock(pthread_rwlock_t* lock) {
    refuseRealtime("pthread write lock");
    static auto actual = reinterpret_cast<int(*)(pthread_rwlock_t*)>(dlsym(RTLD_NEXT, "pthread_rwlock_wrlock"));
    return actual(lock);
}

#endif

// Platform setup substitutes; processing reads the real SharedMemoryPluginBuffer facade.
bool aap::AbstractPluginBuffer::initialize(int32_t ports, int32_t frames) {
    num_ports = ports; num_frames = frames;
    buffers = static_cast<void**>(calloc(ports, sizeof(void*)));
    buffer_sizes = static_cast<int32_t*>(calloc(ports, sizeof(int32_t)));
    return buffers && buffer_sizes;
}
int32_t aap::ClientPluginSharedMemoryStore::allocateClientBuffer(size_t, size_t, PluginInstance&, size_t) {
    throw std::runtime_error("unexpected Android buffer setup");
}
int32_t aap::ServicePluginSharedMemoryStore::allocateServiceBuffer(std::vector<int32_t>&, size_t, PluginInstance&, size_t) {
    throw std::runtime_error("unexpected Android buffer setup");
}
struct TestMemory : PluginSharedMemoryStore {
    explicit TestMemory(PluginInstance& instance) {
        port_buffer = std::make_unique<SharedMemoryPluginBuffer>(&instance);
        port_buffer->initialize(instance.getNumPorts(), 64);
        for (int i = 0; i < instance.getNumPorts(); ++i) {
            auto size = instance.getPort(i)->getContentType() == AAP_CONTENT_TYPE_AUDIO ? 256 : 8192;
            auto data = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
            check(data != MAP_FAILED, "fixture buffer allocation");
            port_buffer->setBuffer(i, data); port_buffer->setBufferSize(i, size);
        }
    }
};
struct TestLocal : LocalPluginInstance {
    using LocalPluginInstance::LocalPluginInstance;
    std::function<void()> beforeWorkerWait;
    std::chrono::steady_clock::time_point nextExtensionDeadline() override {
        if (beforeWorkerWait) beforeWorkerWait();
        return std::chrono::steady_clock::time_point::max();
    }
    void setupTestBuffer() {
        delete shared_memory_store; shared_memory_store = new TestMemory(*this);
        instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
    }
};
struct TestRemote : RemotePluginInstance {
    using RemotePluginInstance::RemotePluginInstance;
    std::function<void()> beforeWorkerWait;
    std::chrono::steady_clock::time_point nextExtensionDeadline() override {
        if (beforeWorkerWait) beforeWorkerWait();
        return RemotePluginInstance::nextExtensionDeadline();
    }
    void setupTestBuffer() {
        delete shared_memory_store; shared_memory_store = new TestMemory(*this);
        instantiation_state = PLUGIN_INSTANTIATION_STATE_ACTIVE;
    }
};
struct Callback : AudioPluginServiceCallback {
    std::atomic<int> process_requests{0};
    void hostExtension(int32_t, const std::string&, int32_t, int32_t, void*) override {}
    void requestProcess(int32_t) override {
        check(!RealtimeScope::isActive(), "request_process delivered on worker"); ++process_requests;
    }
};
struct FakePlugin {
    AndroidAudioPluginHost* host{nullptr};
    std::atomic<int> blocks{0};
    bool notifications{false};
    bool echo{false};
    bool metadataReplies{false};
    bool negotiatedMetadata{false};
    std::atomic<int> metadataRequests{0};
    AAPXSSerializationContext* metadataShared{nullptr};
    bool shortMetadataReply{false};
    std::atomic<bool> shortReplyArmed{false};
    std::atomic<bool> shortReplyReturned{false};
    RealtimeByteQueue<64> legacyReplies{2048};
    xs::TypedAAPXS* typed{nullptr};
    std::atomic<bool>* released{nullptr};
    bool notifyOnRelease{false};
    bool exposeParameters{false};
    aap_parameters_extension_t parameters{this,
        [](auto*, auto*) { return 1; },
        [](auto*, auto*, int32_t) { return aap_parameter_info_t{17, "deferred-layout", "", 0, 100, 30, false}; },
        [](auto*, auto*, int32_t, int32_t) { return 0.0; },
        [](auto*, auto*, int32_t) { return 0; },
        [](auto*, auto*, int32_t, int32_t) { return aap_parameter_enum_t{}; }};
    AndroidAudioPlugin api{this, [](auto*, auto, auto*) {}, [](auto*) {},
        [](auto* plugin, aap_buffer_t* buffer, int32_t, int64_t) {
            auto& self = *static_cast<FakePlugin*>(plugin->plugin_specific);
            check(RealtimeScope::isActive(), "DSP runs inside realtime guard");
            ++self.blocks;
            auto output = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 2));
            if (self.metadataReplies) {
                output->length = 0;
                while (self.legacyReplies.tryConsume([&](void* data, size_t size) {
                    check(output->length + size <= 8192 - sizeof(*output), "legacy reply buffer never overflows");
                    memcpy(reinterpret_cast<uint8_t*>(output + 1) + output->length, data, size);
                    output->length += size;
                    return true;
                })) {}
                auto input = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 1));
                check(self.negotiatedMetadata || input->length == 0, "unacknowledged legacy parser receives no SysEx8 requests");
                if (input->length) {
                    uint8_t payload[1024]{}, conversion[2048]{};
                    aap_midi2_aapxs_parse_context request{};
                    aap_midi2_aapxs_parse_context_prepare(&request, payload, conversion, sizeof(conversion));
                    check(aap_midi2_parse_aapxs_sysex8(&request, reinterpret_cast<uint8_t*>(input + 1), input->length), "legacy metadata request parses");
                    ++self.metadataRequests;
                    int32_t value{}, size = sizeof(value);
                    aap_parameter_info_t parameter{};
                    aap_parameter_enum_t enumeration{};
                    const void* reply = &value;
                    switch (request.opcode) {
                        case OPCODE_PARAMETERS_GET_PARAMETER_COUNT:
                            value = 80;
                            if (self.shortReplyArmed) { size = 1; self.shortReplyReturned = true; }
                            break;
                        case OPCODE_PARAMETERS_GET_PARAMETER:
                            check(request.dataSize == 4, "legacy parameter index width");
                            memcpy(&value, payload, 4);
                            parameter = {static_cast<int16_t>(value), "legacy-layout", "", 0, 100, 30, false};
                            reply = &parameter; size = sizeof(parameter); break;
                        case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT:
                            check(request.dataSize == 4, "legacy enumeration ID width");
                            value = 2; break;
                        case OPCODE_PARAMETERS_GET_ENUMERATION:
                            check(request.dataSize == 8, "legacy enumeration record width");
                            memcpy(&value, payload + 4, 4);
                            enumeration.value = value;
                            strcpy(enumeration.name, "legacy-enum");
                            reply = &enumeration; size = sizeof(enumeration); break;
                        default: check(false, "unexpected metadata opcode");
                    }
                    output->length += aap_midi2_generate_aapxs_sysex8(reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(output + 1) + output->length),
                        (8192 - sizeof(*output) - output->length) / 4, conversion, sizeof(conversion), 0,
                        request.request_id, request.urid, request.uri, request.opcode,
                        static_cast<const uint8_t*>(reply), size);
                    check(output->length > 0, "legacy metadata reply encoded");
                    input->length = 0;
                }
            } else if (self.echo) {
                auto input = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 1));
                memcpy(output, input, sizeof(*input) + input->length);
            } else {
                uint32_t parameter[]{0x40B01100, UINT32_MAX};
                output->length = sizeof(parameter); memcpy(output + 1, parameter, sizeof(parameter));
            }
            auto audio = static_cast<float*>(buffer->get_buffer(buffer, 0));
            for (int i = 0; i < 64; ++i) audio[i] = 0.75f;
            if (self.notifications) {
                self.host->request_process(self.host);
                auto parameters = static_cast<aap_parameters_host_extension_t*>(self.host->get_extension(self.host, AAP_PARAMETERS_EXTENSION_URI));
                parameters->notify_parameters_changed(parameters, self.host);
                auto presets = static_cast<aap_presets_host_extension_t*>(self.host->get_extension(self.host, AAP_PRESETS_EXTENSION_URI));
                presets->notify_preset_loaded(presets, self.host);
                presets->notify_presets_updated(presets, self.host);
                // First and repeated lookups must return the same prebuilt proxy.
                check(parameters == self.host->get_extension(self.host, AAP_PARAMETERS_EXTENSION_URI), "host proxy lookup is cached");
            }
            if (self.typed) {
                auto result = self.typed->callAndWait<int>(1, nullptr, 0, [](auto*) { return 1; });
                check(!result.isOk() && result.error == "RT caller", "sync typed call refused without locks");
                check(self.typed->callFunctionAsync(1, nullptr, 0, [](auto&, auto*, void*) {}) == -1, "async typed call refused before acceptance");
            }
        }, [](auto*) {}, [](auto* plugin, const char* uri) -> void* {
            auto& self = *static_cast<FakePlugin*>(plugin->plugin_specific);
            return self.exposeParameters && !strcmp(uri, AAP_PARAMETERS_EXTENSION_URI) ? &self.parameters : nullptr;
        }, nullptr};
    AndroidAudioPluginFactory factory{
        [](auto* factory, const char*, auto* host) -> AndroidAudioPlugin* {
            auto* self = static_cast<FakePlugin*>(factory->factory_context); self->host = host; return &self->api;
        }, [](auto* factory, auto*) {
            auto& self = *static_cast<FakePlugin*>(factory->factory_context);
            if (self.notifyOnRelease) {
                auto* parameters = static_cast<aap_parameters_host_extension_t*>(self.host->get_extension(self.host, AAP_PARAMETERS_EXTENSION_URI));
                check(parameters, "host facade and proxy survive plugin release");
                parameters->notify_parameters_changed(parameters, self.host);
            }
            if (self.released) self.released->store(true, std::memory_order_release);
        }, this};
};
struct FixtureInfo {
    PluginInformation info{false, "test", "Test", "Test", "", "1", "test", "", "", "", "Instrument", "", "", ""};
    PortInformation audio{0, "audio", AAP_CONTENT_TYPE_AUDIO, AAP_PORT_DIRECTION_OUTPUT};
    PortInformation input{1, "in", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
    PortInformation output{2, "out", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_OUTPUT};
    ParameterInformation parameter{17, "parameter", 0, 100, 30};
    FixtureInfo() { info.addDeclaredPort(&audio); info.addDeclaredPort(&input); info.addDeclaredPort(&output); info.addDeclaredParameter(&parameter); }
};
std::atomic<int> notifications{0};
void notification(void*, const char*, int32_t, int32_t, int32_t, aapxs_completion_callback completed, void* context, void* host, aapxs_error_callback) {
    check(!RealtimeScope::isActive(), "host notification IPC stays off processing"); ++notifications;
    if (completed) completed(context, host);
}
void localProcessing() {
    FixtureInfo descriptor;
    PluginListSnapshot list;
    Callback callback;
    PluginService host(&list, &callback);
    FakePlugin fake; fake.notifications = true;
    TestLocal instance(&host, xs::AAPXSDefinitionRegistry::getStandardExtensions(), 1, &descriptor.info, &fake.factory, 8192);
    instance.setIpcExtensionMessageSender(notification, nullptr);
    // Allocate actual extension blocks used by production setup/proxy construction.
    auto store = instance.getSharedMemoryStore();
    for (auto& definition : *instance.getAAPXSRegistry()->items()) {
        if (!definition.uri || definition.data_capacity == 0) continue;
        auto index = store->getExtensionBufferCount();
        store->addExtensionFD(-1, definition.data_capacity);
        store->getExtensionUriToIndexMap()[definition.uri] = index;
    }
    instance.setupAAPXSInstances(); instance.completeInstantiation(); instance.setupAAPXS(); instance.setupTestBuffer();
    uint32_t raw[]{0x20903C7F};
    instance.addEventUmpInput(raw, sizeof(raw));
    instance.process(64, 0);
    // Stop additional notifications while checking the queue/readback and suspension behavior.
    fake.notifications = false;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while ((notifications < 3 || callback.process_requests < 1) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    check(notifications >= 3 && callback.process_requests >= 1, "all standard notifications delivered");
    // Notification delivery precedes the worker's parameter scan. Join it before
    // testing explicit control exclusion, so a second control operation cannot
    // legitimately suspend the block we expect to resume.
    instance.stopExtensionWorker();
    check(instance.getParameterValueCache().getByIndex(0) == 100, "local output cache updated");
    uint32_t read[2]{};
    extern int32_t readLocalGuiListenerMidi2Output(LocalPluginInstance*, void*, int32_t);
    check(readLocalGuiListenerMidi2Output(&instance, read, 4) == 4, "partial GUI queue read");
    check(readLocalGuiListenerMidi2Output(&instance, read + 1, 4) == 4 && read[0] == 0x40B01100, "GUI remainder retained");
    auto before = fake.blocks.load();
    {
        ProcessingQuiescence::Control suspension(instance.getRealtimeState().processing);
        instance.process(64, 0);
        auto audio = static_cast<float*>(instance.getAudioPluginBuffer()->get_buffer(instance.getAudioPluginBuffer(), 0));
        check(audio[0] == 0 && fake.blocks == before, "suspended block is silent and skips DSP");
    }
    instance.process(64, 0);
    check(fake.blocks > before, "DSP resumes after control work");
    instance.stopExtensionWorker();
    // Context release follows the same order as PluginHost::destroyInstance.
    for (const char* uri : {AAP_PARAMETERS_EXTENSION_URI, AAP_PRESETS_EXTENSION_URI}) {
        auto context = instance.getAAPXSDispatcher().getHostAAPXSByUri(uri);
        instance.getAAPXSRegistry()->items()->getByUri(uri)->release_instance_context(
                instance.getAAPXSRegistry()->items()->getByUri(uri), context->aapxs_context);
        context->aapxs_context = nullptr;
    }
}
void layoutReadiness() {
    FixtureInfo descriptor;
    PluginListSnapshot list;
    Callback callback;
    PluginService host(&list, &callback);
    FakePlugin fake; fake.exposeParameters = true;
    TestLocal instance(&host, xs::AAPXSDefinitionRegistry::getStandardExtensions(), 4, &descriptor.info, &fake.factory, 8192);
    auto store = instance.getSharedMemoryStore();
    for (auto& definition : *instance.getAAPXSRegistry()->items()) {
        if (!definition.uri || definition.data_capacity == 0) continue;
        auto index = store->getExtensionBufferCount();
        store->addExtensionFD(-1, definition.data_capacity);
        store->getExtensionUriToIndexMap()[definition.uri] = index;
    }
    std::promise<void> attempted;
    bool signaled = false; // worker only; callback installed before start
    instance.beforeWorkerWait = [&] { if (!signaled) { signaled = true; attempted.set_value(); } };
    instance.completeInstantiation();
    internal::requestParameterLayoutRefresh(instance);
    instance.setupAAPXSInstances();
    check(attempted.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready,
          "early layout request dispatched before extensions are ready");
    instance.setupAAPXS(); // readiness must wake the sleeping worker and preserve that request
    auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (strcmp(instance.getParameter(0)->getName(), "deferred-layout") && std::chrono::steady_clock::now() < limit)
        std::this_thread::yield();
    check(!strcmp(instance.getParameter(0)->getName(), "deferred-layout"), "early layout request publishes after readiness without audio or another notification");
    instance.stopExtensionWorker();
    for (const char* uri : {AAP_PARAMETERS_EXTENSION_URI, AAP_PRESETS_EXTENSION_URI}) {
        auto context = instance.getAAPXSDispatcher().getHostAAPXSByUri(uri);
        instance.getAAPXSRegistry()->items()->getByUri(uri)->release_instance_context(
                instance.getAAPXSRegistry()->items()->getByUri(uri), context->aapxs_context);
        context->aapxs_context = nullptr;
    }
}
void sharedTransportNegotiation() {
    auto* registry = xs::AAPXSDefinitionRegistry::getStandardExtensions();
    const char* uri = AAP_PARAMETERS_EXTENSION_URI;
    auto urid = registry->getUridMapping()->getUrid(uri);
    std::map<std::string, std::vector<uint8_t>> mappings;
    xs::AAPXSClientDispatcher client(registry);
    check(client.setupInstances(nullptr, [&](const char* key, auto* context) {
        auto& storage = mappings[key]; storage.resize(context->data_capacity);
        context->data = storage.data(); return true;
    }, nullptr, nullptr, nullptr), "negotiating host setup");
    auto* forward = client.getPluginAAPXSByUri(uri)->serialization;
    auto* reverse = client.getHostAAPXSByUri(uri)->serialization;
    check(forward != reverse && forward->data == reverse->data, "old peer keeps offset-zero payload view");
    check(client.getTransportCapabilities() == 0, "old plugin never acknowledges capabilities");
    auto& negotiation = mappings[AAPXS_TRANSPORT_URI];
    check(aapxsTransportMappedSize(negotiation.data(), negotiation.size(), forward->data_capacity,
              mappings[uri].size()) == mappings[uri].size(), "opted-in service maps reserved physical prefix");
    check(aapxsTransportMappedSize(nullptr, 0, forward->data_capacity, mappings[uri].size()) == forward->data_capacity,
          "legacy service still maps advertised logical capacity");
    check(aapxsTransportMappedSize(negotiation.data(), negotiation.size(), forward->data_capacity, forward->data_capacity) == forward->data_capacity,
          "short physical mapping cannot supply reserved prefix");
    auto assign = [&](const char* key, auto* context) {
        auto it = mappings.find(key);
        if (it != mappings.end()) { context->data = it->second.data(); context->data_capacity = it->second.size(); }
    };
    xs::AAPXSServiceDispatcher service(registry);
    service.setupInstances(nullptr, assign, nullptr, nullptr, nullptr);
    client.refreshTransport();
    check(client.getTransportCapabilities() == AAPXS_TRANSPORT_SUPPORTED, "new peer acknowledges transport");
    check(forward->data == service.getPluginAAPXSByUri(uri)->serialization->data, "forward mapping matches across peers");
    check(reverse->data == service.getHostAAPXSByUri(uri)->serialization->data, "reverse mapping matches across peers");
    check(forward->data != reverse->data && forward->data_capacity == registry->getByUri(uri)->data_capacity, "independent payloads with original logical capacity");
    check(registry->getUridMapping()->getUrid(uri) == urid && registry->getUridMapping()->getUrid(AAPXS_TRANSPORT_URI) == 0, "optional FDs do not change wire URIDs");
    std::promise<void> forwardWritten, reverseWritten;
    auto forwardReady = forwardWritten.get_future().share(), reverseReady = reverseWritten.get_future().share();
    auto makeChannel = [&](auto* block, std::promise<void>& written, auto otherReady) {
        return std::make_shared<AAPXSBinderChannel>(block, [&written, otherReady](const auto& request) {
            written.set_value(); otherReady.wait();
            request.callback(request.callback_user_data, nullptr); return true;
        });
    };
    auto forwardChannel = makeChannel(forward, forwardWritten, reverseReady);
    auto reverseChannel = makeChannel(reverse, reverseWritten, forwardReady);
    uint32_t a = 111, b = 222;
    AAPXSSerializationContext aData{&a, 4, 4}, bData{&b, 4, 4};
    std::atomic<int> replies{0};
    auto completed = [](void* context, void*) { ++*static_cast<std::atomic<int>*>(context); };
    AAPXSRequestContext aRequest{completed, &replies, &aData, urid, uri, 101, 7};
    AAPXSRequestContext bRequest{completed, &replies, &bData, urid, uri, 102, -7};
    std::thread aThread([&] { check(forwardChannel->send(&aRequest), "forward request accepted"); });
    std::thread bThread([&] { check(reverseChannel->send(&bRequest), "reverse request accepted"); });
    aThread.join(); bThread.join();
    check(replies == 2 && a == 111 && b == 222, "simultaneous directions cannot overwrite each other");

    // Actual size metadata keeps a small reply from copying a whole 1 MiB block.
    std::vector<uint8_t> replyBuffer(forward->data_capacity, 0x99);
    AAPXSSerializationContext ownReply{replyBuffer.data(), 4, replyBuffer.size()};
    uint32_t input = 42; memcpy(ownReply.data, &input, 4);
    auto exact = std::make_shared<AAPXSBinderChannel>(forward, [&](const auto& request) {
        client.publishBinderRequestSize(forward);
        auto* incoming = service.getPluginAAPXSByUri(uri)->serialization;
        service.receiveBinderRequest(incoming);
        check(incoming->data_size == 4 && *static_cast<uint32_t*>(incoming->data) == 42, "actual request size conveyed without changing POD");
        memset(incoming->data, 0x55, incoming->data_capacity);
        uint32_t result = 123; memcpy(incoming->data, &result, 4); incoming->data_size = 4;
        service.publishBinderReplySize(incoming);
        request.callback(request.callback_user_data, nullptr); return true;
    }, [&]() -> std::optional<size_t> { return client.getBinderReplySize(forward); });
    AAPXSRequestContext exactRequest{completed, &replies, &ownReply, urid, uri, 103, 7};
    check(exact->send(&exactRequest), "length-aware request accepted");
    check(ownReply.data_size == 4 && *static_cast<uint32_t*>(ownReply.data) == 123 && replyBuffer[4] == 0x99 && replyBuffer.back() == 0x99,
          "small reply leaves the rest of the request buffer untouched");
    reverse->data_size = 0;
    client.publishBinderRequestSize(reverse);
    auto* reversePeer = service.getHostAAPXSByUri(uri)->serialization;
    service.receiveBinderRequest(reversePeer);
    check(reversePeer->data_size == 0, "empty reverse request length");
    reversePeer->data_size = 0; service.publishBinderReplySize(reversePeer);
    check(client.getBinderReplySize(reverse) == 0, "empty reverse reply length");
    forward->data_size = forward->data_capacity + 1;
    client.publishBinderRequestSize(forward);
    bool rejected = false;
    try { service.receiveBinderRequest(service.getPluginAAPXSByUri(uri)->serialization); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "oversized request header rejected before handler access");
    forward->data_size = 0;
    client.publishBinderRequestSize(forward);
    auto* forwardPeer = service.getPluginAAPXSByUri(uri)->serialization;
    forwardPeer->data_size = forwardPeer->data_capacity + 1;
    rejected = false;
    try { service.publishBinderReplySize(forwardPeer); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "oversized reply length rejected before successful Binder completion");
    // A new service with an old host has only the original logical-sized block.
    xs::AAPXSServiceDispatcher oldHostService(registry);
    std::vector<uint8_t> oldBlock(registry->getByUri(uri)->data_capacity);
    oldHostService.setupInstances(nullptr, [&](const char* key, auto* context) {
        if (!strcmp(key, uri)) { context->data = oldBlock.data(); context->data_capacity = oldBlock.size(); }
    }, nullptr, nullptr, nullptr);
    check(oldHostService.getTransportCapabilities() == 0 &&
          oldHostService.getPluginAAPXSByUri(uri)->serialization->data == oldBlock.data() &&
          oldHostService.getHostAAPXSByUri(uri)->serialization->data == oldBlock.data(), "old host retains unchanged layout");
    // Missing reverse mapping disables directional negotiation at both ends.
    mappings.erase(aapxsHostDirectionUri(uri));
    xs::AAPXSServiceDispatcher incomplete(registry);
    incomplete.setupInstances(nullptr, assign, nullptr, nullptr, nullptr);
    check(!(incomplete.getTransportCapabilities() & AAPXS_TRANSPORT_DIRECTIONAL), "truncated transport cannot select out-of-bounds views");
}
void remoteProcessing() {
    FixtureInfo descriptor;
    FakePlugin fake; fake.echo = true;
    TestRemote instance(nullptr, xs::AAPXSDefinitionRegistry::getStandardExtensions(), &descriptor.info, &fake.factory, 8192);
    std::vector<std::vector<uint8_t>> blocks;
    void* transportDescriptor = nullptr;
    check(instance.setupAAPXSInstances([&](const char* uri, AAPXSSerializationContext* serialization) {
        blocks.emplace_back(serialization->data_capacity);
        serialization->data = blocks.back().data();
        if (!strcmp(uri, AAPXS_TRANSPORT_URI)) transportDescriptor = serialization->data;
        return true;
    }), "remote setup");
    __atomic_store_n(static_cast<uint32_t*>(transportDescriptor) + 3, AAPXS_TRANSPORT_SYSEX8, __ATOMIC_RELEASE);
    instance.getAAPXSDispatcher().refreshTransport();
    instance.setInstanceId(2); instance.completeInstantiation(); instance.setupTestBuffer();
    auto& dispatcher = instance.getAAPXSDispatcher();
    auto initiator = dispatcher.getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI);
    xs::TypedAAPXS typed(AAP_PARAMETERS_EXTENSION_URI, initiator, initiator->serialization);
    fake.typed = &typed;
    std::promise<void> entered, release;
    auto resume = release.get_future().share();
    std::atomic<int> replies{0};
    uint32_t index = 0;
    typed.callFunctionAsync(OPCODE_PARAMETERS_GET_PARAMETER_COUNT, &index, sizeof(index),
        [&](const std::string& error, auto*, void*) {
            check(!RealtimeScope::isActive() && error.empty(), "remote completion on worker");
            ++replies; entered.set_value(); resume.wait();
        }, sizeof(uint32_t));
    instance.process(64, 0);
    check(entered.get_future().wait_for(std::chrono::seconds(2)) == std::future_status::ready, "reply arrives on worker");
    // A blocked user completion must not prevent another audio block.
    auto before = fake.blocks.load();
    instance.process(64, 0);
    check(fake.blocks == before + 1 && replies == 1, "processing continues during blocked callback");
    release.set_value(); instance.stopExtensionWorker(); fake.typed = nullptr;
}
void activeLayoutRefresh(bool negotiated) {
    FixtureInfo descriptor;
    FakePlugin fake; fake.metadataReplies = true;
    fake.negotiatedMetadata = negotiated;
    TestRemote instance(nullptr, xs::AAPXSDefinitionRegistry::getStandardExtensions(), &descriptor.info, &fake.factory, 8192);
    std::vector<std::vector<uint8_t>> blocks;
    void* transportDescriptor = nullptr;
    check(instance.setupAAPXSInstances([&](const char* uri, AAPXSSerializationContext* serialization) {
        blocks.emplace_back(serialization->data_capacity);
        serialization->data = blocks.back().data();
        if (!strcmp(uri, AAPXS_TRANSPORT_URI)) transportDescriptor = serialization->data;
        return true;
    }), "legacy remote setup");
    if (negotiated) {
        __atomic_store_n(static_cast<uint32_t*>(transportDescriptor) + 3, AAPXS_TRANSPORT_SYSEX8, __ATOMIC_RELEASE);
        instance.getAAPXSDispatcher().refreshTransport();
    }
    instance.setInstanceId(5); instance.completeInstantiation(); instance.setupTestBuffer();
    fake.metadataShared = instance.getAAPXSDispatcher().getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI)->serialization;
    instance.setIpcExtensionMessageSender([](void* context, const char* uri, int32_t, int32_t size,
            int32_t requestId, int32_t opcode, aapxs_completion_callback callback, void* callbackContext, aapxs_error_callback errorCallback) {
        auto& fake = *static_cast<FakePlugin*>(context);
        check(!RealtimeScope::isActive(), "unsafe metadata stays off processing");
        ++fake.metadataRequests;
        auto shared = fake.metadataShared;
        int32_t index{}; if (size) memcpy(&index, shared->data, sizeof(index));
        aap_parameter_info_t parameter{};
        aap_parameter_enum_t enumeration{};
        int32_t value = 2;
        const void* reply{}; size_t replySize{};
        switch (opcode) {
            case OPCODE_PARAMETERS_GET_PARAMETER_COUNT:
                if (fake.shortReplyArmed) {
                    fake.shortReplyReturned = true;
                    errorCallback(callbackContext, &fake.api, "short parameter reply");
                    return true;
                }
                value = 80; reply = &value; replySize = sizeof(value); break;
            case OPCODE_PARAMETERS_GET_PARAMETER:
                check(size == 4, "Binder parameter index width");
                parameter = {static_cast<int16_t>(index), "legacy-layout", "", 0, 100, 30, false};
                reply = &parameter; replySize = sizeof(parameter); break;
            case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT:
                check(size == 4, "Binder enumeration ID width");
                reply = &value; replySize = sizeof(value); break;
            case OPCODE_PARAMETERS_GET_ENUMERATION:
                check(size == 8, "Binder enumeration record width");
                memcpy(&index, static_cast<uint8_t*>(shared->data) + 4, 4);
                enumeration.value = index; strcpy(enumeration.name, "legacy-enum");
                reply = &enumeration; replySize = sizeof(enumeration); break;
            default: check(false, "unexpected legacy Binder opcode");
        }
        // Only the older service mirrors Binder replies into its MIDI output.
        memcpy(shared->data, reply, replySize); shared->data_size = replySize;
        if (fake.shortMetadataReply && opcode == OPCODE_PARAMETERS_GET_PARAMETER && parameter.stable_id == 79) {
            fake.shortReplyArmed = true;
        }
        if (!fake.negotiatedMetadata) {
            uint32_t ump[512]{}; uint8_t conversion[2048]{};
            auto length = aap_midi2_generate_aapxs_sysex8(ump, 512, conversion, sizeof(conversion),
                0, requestId, 0, uri, opcode, static_cast<const uint8_t*>(reply), replySize);
            check(length && fake.legacyReplies.tryPush(ump, length), "legacy reply queue stays bounded");
        }
        callback(callbackContext, &fake.api);
        return true;
    });
    std::atomic<int> changes{0};
    std::atomic<bool> failedScanSettled{false};
    instance.beforeWorkerWait = [&] {
        if (fake.shortReplyReturned && !instance.getRealtimeState().layout_scan) failedScanSettled = true;
    };
    instance.parametersChangedHandler = [&](auto& remote) {
        check(remote.getNumParameters() == 80, "publish complete legacy layout");
        ++changes;
    };
    internal::requestParameterLayoutRefresh(instance);
    // No audio: the scan must remain asynchronous, without Binder reads or
    // partial publication. Actual processing then drives one request per reply.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    check(changes == 0 && instance.getNumParameters() == 1, "old layout retained without audio progress");
    if (negotiated) {
        // One audio exchange delivers the initial RT-safe count. Every remaining
        // read is Binder-only and must finish without further audio progress.
        instance.process(64, 0);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (changes == 0 && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        instance.stopExtensionWorker();
        check(changes == 1 && fake.metadataRequests == 321,
              "negotiated active scan finishes without per-read MIDI drain barriers");
        check(!strcmp(instance.getParameter(79)->getName(), "legacy-layout"), "complete negotiated layout published");
        return;
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (changes == 0 && std::chrono::steady_clock::now() < deadline) {
        instance.process(64, 0);
        std::this_thread::yield();
    }
    check(changes == 1 && fake.metadataRequests == 641, "active scan completes metadata reads with a MIDI drain barrier between Binder reads");
    auto* previous = instance.getParameter(0);
    fake.shortMetadataReply = true;
    internal::requestParameterLayoutRefresh(instance);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (!failedScanSettled && std::chrono::steady_clock::now() < deadline) {
        instance.process(64, 0);
        std::this_thread::yield();
    }
    check(failedScanSettled, "late short metadata reply delivered after partial scan progress");
    instance.stopExtensionWorker();
    check(changes == 1 && instance.getNumParameters() == 80 && instance.getParameter(0) == previous,
          "failed asynchronous scan retains complete previous layout without a changed callback");
    check(!strcmp(instance.getParameter(79)->getName(), "legacy-layout"), "last parameter published");
}
void guardProbes() {
    rlimit noCore{0, 0};
    setrlimit(RLIMIT_CORE, &noCore);
    for (int operation = 0; operation < 3; ++operation) {
        auto child = fork();
        check(child >= 0, "guard probe fork");
        if (child == 0) {
            freopen("/dev/null", "w", stderr);
            RealtimeScope rt;
            if (operation == 0) { std::mutex mutex; mutex.lock(); }
            if (operation == 1) { NanoSleepLock mutex; mutex.try_lock(); }
            if (operation == 2) { auto* data = ::operator new(1); (void) data; }
            _exit(0);
        }
        int status{};
        waitpid(child, &status, 0);
        bool caught = WIFSIGNALED(status) && (WTERMSIG(status) == SIGABRT || WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
        if (!caught)
            fprintf(stderr, "Guard probe %d returned status %d\n", operation, status);
        check(caught, "guard catches lock/try-lock/allocation probes");
    }
}
void sharedObjectScope() {
    auto* path = getenv("AAPXS_RT_SCOPE_DSO_PATH");
    if (!path) return;
    auto* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    check(handle, "hidden SDK consumer loads");
    auto consumer = reinterpret_cast<bool(*)()>(dlsym(handle, "aap_rt_scope_consumer"));
    check(consumer && !consumer(), "consumer starts outside processing");
    {
        RealtimeScope scope;
        check(consumer(), "hidden SDK consumer shares core processing annotation");
        { RealtimeScope nested; check(consumer(), "nested processing scope"); }
        check(consumer(), "nested exit retains outer scope");
    }
    check(!consumer(), "processing annotation leaves shared object");
    dlclose(handle);
}
struct OwningService : PluginService {
    using PluginService::PluginService;
    void adopt(PluginInstance* instance) { instances.push_back(instance); }
};
struct ControlRequest {
    std::atomic<bool> entered{false}, release{false};
    std::atomic<bool> destroyed{false};
    PluginHost* host{nullptr};
    PluginInstance* instance{nullptr};
    bool destroy{false};
    AAPXSDefinition definition{this, "urn:aap:rt-control-test", 4,
        [](auto* definition, auto*, auto*, auto*) {
            auto& self = *static_cast<ControlRequest*>(definition->aapxs_context);
            check(!RealtimeScope::isActive(), "incoming extension runs off processing");
            self.entered = true;
            if (self.destroy) self.host->destroyInstance(self.instance);
            else while (!self.release) std::this_thread::yield();
        }};
    struct TeardownMarker { std::atomic<bool>* completed; };
    ControlRequest() {
        definition.get_host_extension_proxy = [](auto* definition, auto* initiator, auto*) {
            auto* self = static_cast<ControlRequest*>(definition->aapxs_context);
            initiator->aapxs_context = new TeardownMarker{&self->destroyed};
            return AAPXSExtensionServiceProxy{initiator->aapxs_context,
                    [](AAPXSExtensionServiceProxy*) -> void* { return nullptr; }};
        };
        definition.release_instance_context = [](auto*, void* context) {
            auto* marker = static_cast<TeardownMarker*>(context);
            auto* completed = marker->completed;
            delete marker;
            completed->store(true, std::memory_order_release);
        };
    }
};
void deferredRecipientReply() {
    FixtureInfo descriptor;
    PluginListSnapshot list;
    Callback callback;
    struct Requests {
        std::promise<std::shared_ptr<xs::DeferredAAPXSReply>> incoming[2];
        std::atomic<int> received{0};
    } requests;
    AAPXSDefinition definition{&requests, "urn:aap:deferred-reply-test", 4,
        [](auto* def, auto*, auto*, auto* context) {
            check(!RealtimeScope::isActive(), "deferred handler off processing");
            auto reply = xs::retainAAPXSReply(context);
            check(bool(reply), "actual recipient request retainable");
            auto& requests = *static_cast<Requests*>(def->aapxs_context);
            requests.incoming[requests.received++].set_value(reply);
        }};
    std::vector<AAPXSDefinition> definitions;
    for (auto& def : *xs::AAPXSDefinitionRegistry::getStandardExtensions())
        if (def.uri) definitions.push_back(def);
    definitions.push_back(definition);
    xs::AAPXSDefinitionRegistry registry(std::make_unique<xs::UridMapping>(), definitions);
    OwningService host(&list, &callback, &registry);
    FakePlugin fake;
    auto* instance = new TestLocal(&host, &registry, 7, &descriptor.info, &fake.factory, 8192);
    host.adopt(instance);
    auto store = instance->getSharedMemoryStore();
    for (auto& def : registry) {
        if (!def.uri || !def.data_capacity) continue;
        auto index = store->getExtensionBufferCount();
        store->addExtensionFD(-1, def.data_capacity);
        store->getExtensionUriToIndexMap()[def.uri] = index;
    }
    instance->setupAAPXSInstances(); instance->completeInstantiation(); instance->setupAAPXS(); instance->setupTestBuffer();
    uint32_t message[512]{}; uint8_t conversion[2048]{}; uint32_t value = 123;
    auto urid = registry.getUridMapping()->getUrid(definition.uri);
    auto size = aap_midi2_generate_aapxs_sysex8(message, 512, conversion, sizeof(conversion),
            0, 201, urid, definition.uri, 1, reinterpret_cast<uint8_t*>(&value), sizeof(value));
    auto buffer = instance->getAudioPluginBuffer();
    auto input = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 1));
    input->length = size; memcpy(input + 1, message, size);
    instance->process(64, 0);
    auto future = requests.incoming[0].get_future();
    check(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "actual delayed request received");
    auto reply = future.get();
    // The audio port and parser may be reused after dispatch. Complete from a
    // different thread, with only the retained handle, then parse the actual wire.
    memset(input + 1, 0x55, size); input->length = 0;
    std::thread completion([reply] {
        auto& request = reply->request();
        check(*static_cast<uint32_t*>(request.serialization->data) == 123, "incoming payload owned after port reuse");
        *static_cast<uint32_t*>(request.serialization->data) = 321;
        request.serialization->data_size = 4;
        check(reply->complete(), "deferred completion publishes to real instance queue");
        check(!reply->complete(), "real deferred reply publishes once");
    });
    completion.join();
    auto output = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 2));
    output->length = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!output->length && std::chrono::steady_clock::now() < deadline) instance->process(64, 0);
    uint8_t payload[1024]{}, scratch[2048]{};
    aap_midi2_aapxs_parse_context parsed{};
    aap_midi2_aapxs_parse_context_prepare(&parsed, payload, scratch, sizeof(scratch));
    check(output->length && aap_midi2_parse_aapxs_sysex8(&parsed, reinterpret_cast<uint8_t*>(output + 1), output->length), "deferred legacy SysEx8 reply parses");
    check(parsed.request_id == 201 && parsed.urid == urid && parsed.opcode == 1 && parsed.dataSize == 4 &&
            *reinterpret_cast<uint32_t*>(parsed.data) == 321, "deferred reply preserves wire ID/opcode/payload");
    size = aap_midi2_generate_aapxs_sysex8(message, 512, conversion, sizeof(conversion),
            0, 202, urid, definition.uri, 1, reinterpret_cast<uint8_t*>(&value), sizeof(value));
    input->length = size; memcpy(input + 1, message, size);
    instance->process(64, 0);
    auto next = requests.incoming[1].get_future();
    check(next.wait_for(std::chrono::seconds(2)) == std::future_status::ready, "second deferred request received");
    reply = next.get();
    instance->stopExtensionWorker(); // detaches reply target before instance destruction
    host.destroyInstance(instance);
    check(*static_cast<uint32_t*>(reply->request().serialization->data) == 123, "retained data survives instance destruction");
    std::thread late([reply] { check(!reply->complete(), "delayed reply after actual teardown safely refused"); });
    late.join();
}
void incomingControlAndTeardown(bool destroy) {
    FixtureInfo descriptor;
    PluginListSnapshot list;
    Callback callback;
    ControlRequest request; request.destroy = destroy;
    std::vector<AAPXSDefinition> definitions;
    for (auto& definition : *xs::AAPXSDefinitionRegistry::getStandardExtensions())
        if (definition.uri) definitions.push_back(definition);
    definitions.push_back(request.definition);
    xs::AAPXSDefinitionRegistry registry(std::make_unique<xs::UridMapping>(), definitions);
    OwningService host(&list, &callback, &registry);
    FakePlugin fake;
    std::atomic<bool> released{false}; fake.released = &released; fake.notifyOnRelease = true;
    auto* instance = new TestLocal(&host, &registry, 3, &descriptor.info, &fake.factory, 8192);
    host.adopt(instance); request.host = &host; request.instance = instance;
    instance->setIpcExtensionMessageSender(notification, nullptr);
    auto store = instance->getSharedMemoryStore();
    for (auto& definition : registry) {
        if (!definition.uri || definition.data_capacity == 0) continue;
        auto index = store->getExtensionBufferCount();
        store->addExtensionFD(-1, definition.data_capacity);
        store->getExtensionUriToIndexMap()[definition.uri] = index;
    }
    instance->setupAAPXSInstances(); instance->completeInstantiation(); instance->setupAAPXS(); instance->setupTestBuffer();
    uint32_t message[512]{}; uint8_t conversion[2048]{};
    auto size = aap_midi2_generate_aapxs_sysex8(message, 512, conversion, sizeof(conversion),
            0, 99, registry.getUridMapping()->getUrid(request.definition.uri), request.definition.uri, 1, nullptr, 0);
    auto buffer = instance->getAudioPluginBuffer();
    auto header = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 1));
    header->length = size; memcpy(header + 1, message, size);
    instance->process(64, 0); // only copies the request; worker dispatch may overlap later
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!request.entered && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    check(request.entered, "incoming control request delivered");
    if (!destroy) {
        auto before = fake.blocks.load();
        instance->process(64, 0);
        check(fake.blocks == before, "worker control excludes DSP without audio waiting");
        request.release = true;
        instance->stopExtensionWorker();
        instance->process(64, 0);
        check(fake.blocks > before, "processing resumes after incoming control");
        host.destroyInstance(instance);
    }
    while (!request.destroyed && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    check(released && request.destroyed, "worker callback destruction waits for handler, joins and releases all contexts");
}
int main() {
    guardProbes(); sharedObjectScope(); sharedTransportNegotiation(); localProcessing(); layoutReadiness(); remoteProcessing(); activeLayoutRefresh(false); activeLayoutRefresh(true);
    incomingControlAndTeardown(false); incomingControlAndTeardown(true);
    deferredRecipientReply();
    puts("PASS: actual local/remote processing, cached proxies, notifications and suspension without locks or C++ allocation");
}
