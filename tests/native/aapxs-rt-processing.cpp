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
    std::unique_ptr<PluginSharedMemoryStore> extensionOwner;
    explicit TestMemory(PluginInstance& instance, PluginSharedMemoryStore* previous = nullptr)
        : extensionOwner(previous) {
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
        shared_memory_store = new TestMemory(*this, shared_memory_store);
        prepare(64, 48000);
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
        shared_memory_store = new TestMemory(*this, shared_memory_store);
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
    bool captureMidi{false};
    std::array<uint32_t, 4096> receivedMidi{};
    size_t receivedWords{0};
    std::array<uint32_t, 4096> otherMidi{};
    size_t otherWords{0};
    std::atomic<int> metadataRequests{0};
    std::function<bool(int32_t, int32_t, int32_t, aapxs_completion_callback, void*, aapxs_error_callback)> metadataHandler;
    AAPXSSerializationContext* metadataShared{nullptr};
    bool shortMetadataReply{false};
    std::atomic<bool> shortReplyArmed{false};
    std::atomic<bool> shortReplyReturned{false};
    RealtimeByteQueue<64> legacyReplies{2048};
    xs::TypedAAPXS* typed{nullptr};
    std::atomic<bool>* released{nullptr};
    bool notifyOnRelease{false};
    bool notifyOnInstantiate{false};
    bool exposeParameters{false};
    bool exposePresets{false};
    std::atomic<int32_t> parameterCount{1}, presetCount{40};
    std::atomic<int> countReads{0};
    aap_parameters_extension_t parameters{this,
        [](auto* ext, auto*) {
            auto& self = *static_cast<FakePlugin*>(ext->aapxs_context);
            check(!RealtimeScope::isActive(), "count refresh stays off DSP");
            ++self.countReads; return self.parameterCount.load();
        },
        [](auto*, auto*, int32_t index) { return aap_parameter_info_t{static_cast<int16_t>(17 + index), "deferred-layout", "", 0, 100, 30, false}; },
        [](auto*, auto*, int32_t, int32_t) { return 0.0; },
        [](auto*, auto*, int32_t) { return 0; },
        [](auto*, auto*, int32_t, int32_t) { return aap_parameter_enum_t{}; }};
    aap_presets_extension_t presets{this,
        [](auto* ext, auto*) {
            auto& self = *static_cast<FakePlugin*>(ext->aapxs_context);
            check(!RealtimeScope::isActive(), "preset count refresh stays off DSP");
            ++self.countReads; return self.presetCount.load();
        },
        [](auto*, auto*, int32_t, auto*, auto, void*) {},
        [](auto* ext, auto*, int32_t) {
            auto& self = *static_cast<FakePlugin*>(ext->aapxs_context);
            ++self.parameterCount; ++self.presetCount;
        }};
    AndroidAudioPlugin api{this, [](auto*, auto, auto*) {}, [](auto*) {},
        [](auto* plugin, aap_buffer_t* buffer, int32_t, int64_t) {
            auto& self = *static_cast<FakePlugin*>(plugin->plugin_specific);
            check(RealtimeScope::isActive(), "DSP runs inside realtime guard");
            ++self.blocks;
            if (self.captureMidi) {
                auto input = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 1));
                check(self.receivedWords + input->length / 4 <= self.receivedMidi.size(), "fixture MIDI capture capacity");
                memcpy(self.receivedMidi.data() + self.receivedWords, input + 1, input->length);
                self.receivedWords += input->length / 4;
                if (buffer->num_ports(buffer) > 3) {
                    auto other = static_cast<AAPMidiBufferHeader*>(buffer->get_buffer(buffer, 3));
                    check(self.otherWords + other->length / 4 <= self.otherMidi.size(), "fixture second-port capture capacity");
                    memcpy(self.otherMidi.data() + self.otherWords, other + 1, other->length);
                    self.otherWords += other->length / 4;
                }
            }
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
            if (self.exposeParameters && !strcmp(uri, AAP_PARAMETERS_EXTENSION_URI)) return &self.parameters;
            if (self.exposePresets && !strcmp(uri, AAP_PRESETS_EXTENSION_URI)) return &self.presets;
            return nullptr;
        }, nullptr};
    AndroidAudioPluginFactory factory{
        [](auto* factory, const char*, auto* host) -> AndroidAudioPlugin* {
            auto* self = static_cast<FakePlugin*>(factory->factory_context); self->host = host;
            if (self->notifyOnInstantiate) {
                auto parameters = static_cast<aap_parameters_host_extension_t*>(host->get_extension(host, AAP_PARAMETERS_EXTENSION_URI));
                parameters->notify_parameters_changed(parameters, host);
            }
            return &self->api;
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
    PortInformation otherInput{3, "other", AAP_CONTENT_TYPE_MIDI2, AAP_PORT_DIRECTION_INPUT};
    descriptor.info.addDeclaredPort(&otherInput);
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
    // Buses are derived from the ports; only the first MIDI2 port of each direction is the main event bus.
    check(instance.getNumBuses(AAP_BUS_KIND_AUDIO, AAP_PORT_DIRECTION_INPUT) == 0, "no audio input bus");
    auto audioOut = instance.getBus(AAP_BUS_KIND_AUDIO, AAP_PORT_DIRECTION_OUTPUT, 0);
    check(audioOut && audioOut->getRole() == AAP_BUS_ROLE_MAIN && audioOut->getChannelCount() == 1 &&
          audioOut->getPortIndex(0) == 0 && std::string{audioOut->getLayout()} == AAP_BUS_LAYOUT_MONO, "main audio output bus");
    check(instance.getNumBuses(AAP_BUS_KIND_EVENT, AAP_PORT_DIRECTION_INPUT) == 1 &&
          instance.getBus(AAP_BUS_KIND_EVENT, AAP_PORT_DIRECTION_INPUT, 0)->getPortIndex() == 1 &&
          instance.getMainEventPortIndex(AAP_PORT_DIRECTION_INPUT) == 1, "main event input bus");
    check(instance.getMainEventPortIndex(AAP_PORT_DIRECTION_OUTPUT) == 2, "main event output bus");
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
    fake.captureMidi = true;
    auto midi = static_cast<AAPMidiBufferHeader*>(instance.getAudioPluginBuffer()->get_buffer(instance.getAudioPluginBuffer(), 1));
    auto other = static_cast<AAPMidiBufferHeader*>(instance.getAudioPluginBuffer()->get_buffer(instance.getAudioPluginBuffer(), 3));
    uint32_t queued[]{0x20804600};
    instance.addEventUmpInput(queued, sizeof(queued));
    {
        ProcessingQuiescence::Control suspension(instance.getRealtimeState().processing);
        uint32_t events[]{0x00200040, 0x20803C00}; // late timestamp and note-off
        midi->length = sizeof(events); memcpy(midi + 1, events, sizeof(events));
        other->length = 4; *reinterpret_cast<uint32_t*>(other + 1) = 0x21823D00;
        instance.process(64, 0);
        auto audio = static_cast<float*>(instance.getAudioPluginBuffer()->get_buffer(instance.getAudioPluginBuffer(), 0));
        check(audio[0] == 0 && fake.blocks == before, "suspended block is silent and skips DSP");
        uint32_t second[]{0x40914000, 0x80000000};
        midi->length = sizeof(second); memcpy(midi + 1, second, sizeof(second));
        other->length = 4; *reinterpret_cast<uint32_t*>(other + 1) = 0x2192417F;
        instance.process(64, 0);
        check(midi->length == 0 && other->length == 0 && fake.receivedWords == 0,
              "skipped blocks consume shared inputs without prematurely delivering MIDI");
    }
    uint32_t fresh[]{0x00200020, 0x2090457F};
    midi->length = sizeof(fresh); memcpy(midi + 1, fresh, sizeof(fresh));
    instance.process(64, 0);
    check(fake.blocks > before, "DSP resumes after control work");
    uint32_t expected[]{0x20804600, 0x20803C00, 0x40914000, 0x80000000, 0x00200020, 0x2090457F};
    check(fake.receivedWords == std::size(expected) && !memcmp(fake.receivedMidi.data(), expected, sizeof(expected)),
          "late note-off and MIDI2 note replay in order before fresh timestamped input");
    check(fake.otherWords == 2 && fake.otherMidi[0] == 0x21823D00 && fake.otherMidi[1] == 0x2192417F,
          "retained MIDI keeps its original port and order");
    check(midi->length == 0 && other->length == 0, "all input ports reset after DSP consumes them");
    instance.process(64, 0);
    check(fake.receivedWords == std::size(expected) && fake.otherWords == 2, "retained events delivered once");
    {
        ProcessingQuiescence::Control suspension(instance.getRealtimeState().processing);
        midi->length = 4; *reinterpret_cast<uint32_t*>(midi + 1) = 0x20904A7F;
        instance.process(64, 0);
    }
    instance.deactivate(); instance.activate();
    instance.process(64, 0);
    check(fake.receivedWords == std::size(expected), "deactivation discards stale retained notes before reactivation");
    instance.stopExtensionWorker();
    // Context release follows the same order as PluginHost::destroyInstance.
    for (const char* uri : {AAP_PARAMETERS_EXTENSION_URI, AAP_PRESETS_EXTENSION_URI}) {
        auto context = instance.getAAPXSDispatcher().getHostAAPXSByUri(uri);
        instance.getAAPXSRegistry()->items()->getByUri(uri)->release_instance_context(
                instance.getAAPXSRegistry()->items()->getByUri(uri), context->aapxs_context);
        context->aapxs_context = nullptr;
    }
}
void deferredMidiBuffers() {
    struct Buffer { AAPMidiBufferHeader header{}; std::array<uint32_t, 64> data{}; } buffer;
    DeferredMidiInput input(32);
    DeferredMidiInput recovery(16);
    {
        RealtimeScope realtime;
        buffer.data[0] = 0x00200010; buffer.data[1] = 0x20803C00; buffer.header.length = 8;
        input.capture(buffer.header);
        buffer.data[0] = 0x40813D00; buffer.data[1] = 0; buffer.header.length = 8;
        input.capture(buffer.header);
        buffer.data[0] = 0x00200030; buffer.data[1] = 0x2090407F; buffer.header.length = 8;
        input.capture(buffer.header, false);
        input.drain(buffer.header, 12);
        check(buffer.header.length == 12 && buffer.data[0] == 0x20803C00 && buffer.data[1] == 0x40813D00,
              "drain stops at a complete packet and retains newer input");
        input.age();
        buffer.data[0] = 0x00200040; buffer.data[1] = 0x2090417F; buffer.header.length = 8;
        input.capture(buffer.header, false); input.drain(buffer.header, 32);
        check(buffer.header.length == 12 && buffer.data[0] == 0x2090407F && buffer.data[1] == 0x00200040 && buffer.data[2] == 0x2090417F,
              "carried live input loses stale timing; new live timing remains intact");
        input.reset();
        for (unsigned i = 0; i < 16; ++i) {
            buffer.data[i * 2] = 0x40803C00; buffer.data[i * 2 + 1] = i;
        }
        buffer.header.length = 128; input.capture(buffer.header);
        for (unsigned i = 0; i < 16; ++i) {
            buffer.header.length = 0; input.drain(buffer.header, 8);
            check(buffer.header.length == 8 && buffer.data[1] == i, "FIFO packet order survives ring wrap");
            if (i < 4) {
                buffer.data[0] = 0x40803C00; buffer.data[1] = 16 + i; buffer.header.length = 8;
                input.capture(buffer.header);
            }
        }
        for (unsigned i = 16; i < 20; ++i) {
            buffer.header.length = 0; input.drain(buffer.header, 8);
            check(buffer.header.length == 8 && buffer.data[1] == i, "wrapped append retains full MIDI2 packets");
        }
        check(!input.pending(), "FIFO drains completely");
        buffer.data[0] = 0x20903C7F; buffer.data[1] = 0x43954800; buffer.data[2] = 0x80000000;
        buffer.header.length = 12; recovery.observe(buffer.header);
        bool midi1Off = false, midi2Off = false;
        for (unsigned block = 0; block < 16; ++block) {
            // Repeated overload must not restart recovery and starve later channels.
            for (unsigned i = 0; i < 20; ++i) buffer.data[i] = 0x20904A7F;
            buffer.header.length = 80; recovery.capture(buffer.header);
            recovery.drain(buffer.header, 8);
            for (unsigned offset = 0; offset < buffer.header.length / 4;) {
                auto word = buffer.data[offset];
                midi1Off |= word == 0x20803C00;
                midi2Off |= word == 0x43854800;
                offset += cmidi2_ump_get_num_bytes(word) / 4;
            }
            if (!recovery.pending()) break;
        }
        check(recovery.overflowCount() > 1 && midi1Off && midi2Off && !recovery.pending(),
              "overflow emits explicit MIDI1/MIDI2 note-offs on original groups/channels despite repeated overload");
    }
}
int32_t readCachedCount(TestLocal& instance, const char* uri, int32_t opcode) {
    instance.controlExtension(0, uri, opcode, 900);
    int32_t result{};
    memcpy(&result, instance.getAAPXSDispatcher().getPluginAAPXSByUri(uri)->serialization->data, sizeof(result));
    return result;
}
void countPolling() {
    FixtureInfo descriptor;
    PluginListSnapshot list; Callback callback; PluginService host(&list, &callback);
    FakePlugin fake; fake.exposeParameters = fake.exposePresets = true;
    TestLocal instance(&host, xs::AAPXSDefinitionRegistry::getStandardExtensions(), 9, &descriptor.info, &fake.factory, 8192);
    auto store = instance.getSharedMemoryStore();
    for (auto& definition : *instance.getAAPXSRegistry()->items()) {
        if (!definition.uri || definition.data_capacity == 0) continue;
        auto index = store->getExtensionBufferCount();
        auto backing = tmpfile(); check(backing, "extension backing file");
        int fd = dup(fileno(backing)); fclose(backing);
        check(fd > 0 && ftruncate(fd, definition.data_capacity) == 0, "extension backing size");
        store->addExtensionFD(fd, definition.data_capacity);
        store->getExtensionUriToIndexMap()[definition.uri] = index;
    }
    instance.setupAAPXSInstances(); instance.completeInstantiation(); instance.setupAAPXS(); instance.setupTestBuffer();
    auto& state = instance.getRealtimeState();
    auto countReads = fake.countReads.load();
    {
        auto shared = instance.getAAPXSDispatcher().getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI)->serialization;
        const auto metadataCountReads = fake.countReads.load();
        // A metadata scan must not call unrelated count getters after every read.
        // In JUCE those getters can synchronously wait on a visible editor's thread.
        std::atomic<int> metadataReplies{0};
        state.recipient_requests.setSender([&](const AAPXSRequestContext& request) {
            check(!RealtimeScope::isActive(), "metadata completion stays off DSP");
            if (!strcmp(request.uri, AAP_PARAMETERS_EXTENSION_URI) && request.opcode == OPCODE_PARAMETERS_GET_PARAMETER) {
                aap_parameter_info_t info{}; memcpy(&info, request.serialization->data, sizeof(info));
                check(info.stable_id == 17 && !strcmp(info.display_name, "deferred-layout"), "metadata getter still returns plugin information");
            }
            ++metadataReplies; return true;
        });
        for (const auto opcode : {OPCODE_PARAMETERS_GET_PARAMETER, OPCODE_PARAMETERS_GET_PROPERTY,
                                 OPCODE_PARAMETERS_GET_ENUMERATION_COUNT, OPCODE_PARAMETERS_GET_ENUMERATION}) {
            int32_t payload[]{opcode == OPCODE_PARAMETERS_GET_PARAMETER ? 0 : 17, 0};
            const auto bytes = opcode == OPCODE_PARAMETERS_GET_PROPERTY || opcode == OPCODE_PARAMETERS_GET_ENUMERATION ? 8u : 4u;
            memcpy(shared->data, payload, bytes); shared->data_size = bytes;
            instance.controlExtension(0, AAP_PARAMETERS_EXTENSION_URI, opcode, 200 + opcode);
            aap_midi2_aapxs_parse_context request{};
            strcpy(request.uri, AAP_PARAMETERS_EXTENSION_URI); request.opcode = opcode;
            request.request_id = 300 + opcode; request.data = reinterpret_cast<uint8_t*>(payload); request.dataSize = bytes;
            instance.handleAAPXSInput(&request);
        }
        check(metadataReplies == 4 && fake.countReads == metadataCountReads,
              "Binder and SysEx8 metadata reads do not marshal parameter/preset count getters");
        auto preset = instance.getAAPXSDispatcher().getPluginAAPXSByUri(AAP_PRESETS_EXTENSION_URI)->serialization;
        int32_t presetPayload[]{0, 1}; memcpy(preset->data, presetPayload, sizeof(presetPayload));
        preset->data_size = sizeof(presetPayload);
        instance.controlExtension(0, AAP_PRESETS_EXTENSION_URI, OPCODE_GET_PRESET_DATA, 210);
        aap_midi2_aapxs_parse_context presetRequest{};
        strcpy(presetRequest.uri, AAP_PRESETS_EXTENSION_URI); presetRequest.opcode = OPCODE_GET_PRESET_DATA;
        presetRequest.request_id = 310; presetRequest.data = reinterpret_cast<uint8_t*>(presetPayload);
        presetRequest.dataSize = sizeof(presetPayload); instance.handleAAPXSInput(&presetRequest);
        check(metadataReplies == 5 && fake.countReads == metadataCountReads, "preset-name reads also avoid unrelated count refreshes");
        state.recipient_requests.setSender({});
    }
    std::promise<void> entered, release;
    auto resume = release.get_future().share();
    std::thread controller([&] {
        ProcessingQuiescence::Control suspension(state.processing);
        entered.set_value(); resume.wait();
    });
    entered.get_future().wait();
    auto binder = std::async(std::launch::async, [&] {
        instance.controlExtension(0, AAP_PARAMETERS_EXTENSION_URI, OPCODE_PARAMETERS_GET_PARAMETER_COUNT, 100);
        instance.controlExtension(0, AAP_PRESETS_EXTENSION_URI, OPCODE_GET_PRESET_COUNT, 101);
    });
    auto binderReady = binder.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    std::atomic<int> replies{0};
    std::array<int32_t, 2> values{};
    state.recipient_requests.setSender([&](const AAPXSRequestContext& request) {
        check(!RealtimeScope::isActive(), "cached reply encoding stays off DSP");
        auto i = request.request_id == 102 ? 0 : 1;
        memcpy(&values[i], request.serialization->data, 4);
        ++replies; return true;
    });
    auto input = static_cast<AAPMidiBufferHeader*>(instance.getAudioPluginBuffer()->get_buffer(instance.getAudioPluginBuffer(), 1));
    uint32_t wire[512]{}; uint8_t conversion[2048]{};
    auto size = aap_midi2_generate_aapxs_sysex8(wire, 512, conversion, sizeof(conversion), 0, 102, 0,
            AAP_PARAMETERS_EXTENSION_URI, OPCODE_PARAMETERS_GET_PARAMETER_COUNT, nullptr, 0);
    size += aap_midi2_generate_aapxs_sysex8(wire + size / 4, 512 - size / 4, conversion, sizeof(conversion), 0, 103, 0,
            AAP_PRESETS_EXTENSION_URI, OPCODE_GET_PRESET_COUNT, nullptr, 0);
    input->length = size; memcpy(input + 1, wire, size);
    instance.process(64, 0); // control is paused: handoff must still service polls
    auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (replies < 2 && std::chrono::steady_clock::now() < limit) std::this_thread::yield();
    bool midiReady = replies == 2;
    release.set_value(); controller.join(); binder.get();
    check(binderReady && midiReady && values[0] == 1 && values[1] == 40 && fake.countReads == countReads,
          "Binder and SysEx8 count polls complete during unrelated control without plugin calls or control suspension");
    std::atomic<int> refreshedNotifications{0};
    std::pair<TestLocal*, std::atomic<int>*> notificationContext{&instance, &refreshedNotifications};
    // The callback context is installed before the notification can wake the worker.
    instance.setIpcExtensionMessageSender([](void* context, const char*, int32_t, int32_t, int32_t,
            aapxs_completion_callback completed, void* callbackContext, void* hostContext, aapxs_error_callback) {
        auto& pair = *static_cast<std::pair<TestLocal*, std::atomic<int>*>*>(context);
        check(readCachedCount(*pair.first, AAP_PARAMETERS_EXTENSION_URI, OPCODE_PARAMETERS_GET_PARAMETER_COUNT) == 2 &&
              readCachedCount(*pair.first, AAP_PRESETS_EXTENSION_URI, OPCODE_GET_PRESET_COUNT) == 41,
              "fresh counts precede notification delivery");
        ++*pair.second; if (completed) completed(callbackContext, hostContext);
    }, &notificationContext);
    fake.parameterCount = 2; fake.presetCount = 41;
    {
        RealtimeScope realtime;
        auto parameters = static_cast<aap_parameters_host_extension_t*>(fake.host->get_extension(fake.host, AAP_PARAMETERS_EXTENSION_URI));
        auto presets = static_cast<aap_presets_host_extension_t*>(fake.host->get_extension(fake.host, AAP_PRESETS_EXTENSION_URI));
        parameters->notify_parameters_changed(parameters, fake.host);
        presets->notify_presets_updated(presets, fake.host);
    }
    limit = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (refreshedNotifications < 2 && std::chrono::steady_clock::now() < limit) std::this_thread::yield();
    check(refreshedNotifications == 2, "both count snapshots refreshed after processing notifications");
    instance.stopExtensionWorker();
    countReads = fake.countReads;
    instance.controlExtension(0, AAP_PARAMETERS_EXTENSION_URI, OPCODE_PARAMETERS_GET_PARAMETER_COUNT, 104);
    auto shared = instance.getAAPXSDispatcher().getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI)->serialization;
    int32_t count; memcpy(&count, shared->data, 4);
    check(count == 2 && fake.countReads == countReads, "updated parameter poll reads snapshot without another plugin call");
    auto preset = instance.getAAPXSDispatcher().getPluginAAPXSByUri(AAP_PRESETS_EXTENSION_URI)->serialization;
    int32_t index = 0; memcpy(preset->data, &index, 4);
    instance.controlExtension(0, AAP_PRESETS_EXTENSION_URI, OPCODE_SET_PRESET_INDEX, 105);
    instance.controlExtension(0, AAP_PARAMETERS_EXTENSION_URI, OPCODE_PARAMETERS_GET_PARAMETER_COUNT, 106);
    memcpy(&count, shared->data, 4);
    check(count == 3 && readCachedCount(instance, AAP_PRESETS_EXTENSION_URI, OPCODE_GET_PRESET_COUNT) == 42, "unsafe control mutation refreshes counts even without a plugin notification");
    auto definition = instance.getAAPXSRegistry()->items()->getByUri(AAP_PARAMETERS_EXTENSION_URI);
    auto standardHandler = definition->process_incoming_plugin_aapxs_request;
    definition->process_incoming_plugin_aapxs_request = [](AAPXSDefinition*, AAPXSRecipientInstance* recipient,
            AndroidAudioPlugin*, AAPXSRequestContext* request) {
        int32_t custom = 777; memcpy(request->serialization->data, &custom, 4);
        request->serialization->data_size = 4;
        recipient->send_aapxs_reply(recipient, request);
    };
    instance.controlExtension(0, AAP_PARAMETERS_EXTENSION_URI, OPCODE_PARAMETERS_GET_PARAMETER_COUNT, 107);
    memcpy(&count, shared->data, 4);
    definition->process_incoming_plugin_aapxs_request = standardHandler;
    check(count == 777, "custom handler at the standard URI retains its own behavior instead of a cached reply");
    definition->process_incoming_plugin_aapxs_request = [](AAPXSDefinition*, AAPXSRecipientInstance* recipient,
            AndroidAudioPlugin*, AAPXSRequestContext* request) {
        request->serialization->data_size = 0;
        recipient->send_aapxs_reply(recipient, request);
    };
    countReads = fake.countReads;
    instance.controlExtension(0, AAP_PARAMETERS_EXTENSION_URI, OPCODE_PARAMETERS_GET_PARAMETER, 108);
    definition->process_incoming_plugin_aapxs_request = standardHandler;
    check(fake.countReads > countReads, "custom metadata opcode retains conservative count refresh");
    for (const char* uri : {AAP_PARAMETERS_EXTENSION_URI, AAP_PRESETS_EXTENSION_URI}) {
        auto context = instance.getAAPXSDispatcher().getHostAAPXSByUri(uri);
        instance.getAAPXSRegistry()->items()->getByUri(uri)->release_instance_context(
                instance.getAAPXSRegistry()->items()->getByUri(uri), context->aapxs_context);
        context->aapxs_context = nullptr;
    }
}
void extensionNeutralDispatch() {
    struct Example {
        std::atomic<int32_t> live{23}, snapshot{0};
        std::atomic<int> refreshes{0}, notifications{0};
    } example;
    constexpr auto uri = "urn:test:unrelated-extension";
    AAPXSDefinition definition{&example, uri, 4,
        [](auto* definition, auto* recipient, auto*, auto* request) {
            auto& state = *static_cast<Example*>(definition->aapxs_context);
            int32_t result;
            if (request->opcode == 37) result = state.snapshot.load();
            else if (request->opcode == 38) result = state.live.load();
            else result = ++state.live;
            memcpy(request->serialization->data, &result, 4); request->serialization->data_size = 4;
            recipient->send_aapxs_reply(recipient, request);
        }};
    definition.get_request_flags = [](auto*, bool host, int32_t opcode) -> uint32_t {
        if (host) return opcode == -7 ? AAPXS_REQUEST_COALESCE | AAPXS_REQUEST_STATE_CHANGED : 0;
        if (opcode == 37) return AAPXS_REQUEST_READ_ONLY | AAPXS_REQUEST_CONCURRENT;
        return opcode == 38 ? AAPXS_REQUEST_READ_ONLY : 0;
    };
    definition.on_plugin_state_changed = [](auto* definition, auto*, auto*) {
        auto& state = *static_cast<Example*>(definition->aapxs_context);
        state.snapshot = state.live.load(); ++state.refreshes;
    };
    std::vector<AAPXSDefinition> definitions;
    for (auto& d : *xs::AAPXSDefinitionRegistry::getStandardExtensions()) if (d.uri) definitions.push_back(d);
    definitions.push_back(definition);
    xs::AAPXSDefinitionRegistry registry(std::make_unique<xs::UridMapping>(), definitions);
    FixtureInfo descriptor; PluginListSnapshot list; Callback callback; PluginService host(&list, &callback, &registry);
    FakePlugin fake;
    TestLocal instance(&host, &registry, 4, &descriptor.info, &fake.factory, 8192);
    auto store = instance.getSharedMemoryStore();
    for (auto& d : registry) {
        if (!d.uri || !d.data_capacity) continue;
        auto index = store->getExtensionBufferCount(); auto backing = tmpfile();
        int fd = dup(fileno(backing)); fclose(backing); check(ftruncate(fd, d.data_capacity) == 0, "generic extension backing");
        store->addExtensionFD(fd, d.data_capacity); store->getExtensionUriToIndexMap()[d.uri] = index;
    }
    instance.setupAAPXSInstances(); instance.completeInstantiation(); instance.setupAAPXS(); instance.setupTestBuffer();
    auto refreshes = example.refreshes.load();
    std::promise<void> entered, release; auto resume = release.get_future().share();
    std::thread controller([&] { ProcessingQuiescence::Control gate(instance.getRealtimeState().processing); entered.set_value(); resume.wait(); });
    entered.get_future().wait();
    auto cached = std::async(std::launch::async, [&] { return readCachedCount(instance, uri, 37); });
    const auto ready = cached.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    release.set_value(); controller.join();
    check(ready && cached.get() == 23 && example.refreshes == refreshes, "foreign cached opcode follows declared concurrency/read-only policy");
    example.live = 99;
    check(readCachedCount(instance, uri, 38) == 99 && example.snapshot == 23 && example.refreshes == refreshes,
          "foreign read-only opcode does not invalidate snapshots");
    readCachedCount(instance, uri, 39);
    check(example.snapshot == 100 && example.refreshes > refreshes, "unannotated mutation refreshes extension-owned state");
    instance.setIpcExtensionMessageSender([](void* context, const char* uri, int32_t, int32_t opcode, int32_t,
            aapxs_completion_callback completed, void* completionContext, void* hostContext, aapxs_error_callback) {
        auto& state = *static_cast<Example*>(context);
        check(!strcmp(uri, "urn:test:unrelated-extension") && opcode == -7 && state.snapshot == 101,
              "foreign coalesced notification observes fresh extension-owned snapshot");
        ++state.notifications; if (completed) completed(completionContext, hostContext);
    }, &example);
    example.live = 101;
    AAPXSSerializationContext empty{}; AAPXSRequestContext notification{nullptr, nullptr, &empty, 0, uri, 1, -7};
    { RealtimeScope scope; for (unsigned n = 0; n < 1000; ++n) instance.sendHostAAPXSRequest(&notification); }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (example.notifications == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
    check(example.notifications > 0, "generic coalescer supports a nonstandard URI/opcode without DSP allocation/locks");
    instance.stopExtensionWorker();
}
void layoutReadiness() {
    FixtureInfo descriptor;
    PluginListSnapshot list;
    Callback callback;
    PluginService host(&list, &callback);
    FakePlugin fake; fake.exposeParameters = true; fake.notifyOnInstantiate = true;
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
    instance.setIpcExtensionMessageSender(notification, nullptr);
    instance.setupAAPXSInstances(); // cached host proxies must exist during factory callbacks
    instance.completeInstantiation();
    internal::requestParameterLayoutRefresh(instance);
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
    std::atomic<bool> pausedScanSettled{false};
    instance.beforeWorkerWait = [&] {
        if (fake.shortReplyReturned && !instance.getRealtimeState().layout_scan) failedScanSettled = true;
        if (fake.metadataRequests > 0 && !instance.getRealtimeState().layout_scan &&
            instance.getRealtimeState().layout_refresh.load()) pausedScanSettled = true;
    };
    instance.parametersChangedHandler = [&](auto& remote) {
        check(remote.getNumParameters() == 80, "publish complete legacy layout");
        ++changes;
    };
    internal::requestParameterLayoutRefresh(instance);
    // No audio: the scan must remain asynchronous, without Binder reads or
    // partial publication. Actual processing then drives one request per reply.
    std::this_thread::sleep_for(std::chrono::milliseconds(negotiated ? 20 : 1100));
    check(changes == 0 && instance.getNumParameters() == 1, "old layout retained without audio progress");
    check(fake.metadataRequests == 0, "startup emits no metadata requests without audio");
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
    // Start one read, then stop audio for longer than the pacing timeout. No
    // second notification is sent: the original refresh must survive both the
    // startup gap and a later pause, and complete once processing resumes.
    instance.process(64, 0);
    auto started = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (fake.metadataRequests == 0 && std::chrono::steady_clock::now() < started) std::this_thread::yield();
    check(fake.metadataRequests == 1, "first progress admits one legacy Binder read");
    auto paused = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!pausedScanSettled && std::chrono::steady_clock::now() < paused)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(pausedScanSettled && changes == 0 && fake.metadataRequests == 1,
          "paused legacy scan is retained after timeout without retries or partial publication");
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (changes == 0 && std::chrono::steady_clock::now() < deadline) {
        instance.process(64, 0);
        std::this_thread::yield();
    }
    check(changes == 1 && fake.metadataRequests == 642, "retained scan restarts and completes with a MIDI drain barrier between Binder reads");
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
void supersededLayoutRefresh(bool negotiated, bool failedOldReply) {
    FixtureInfo descriptor;
    FakePlugin fake; fake.metadataReplies = true; fake.negotiatedMetadata = negotiated;
    TestRemote instance(nullptr, xs::AAPXSDefinitionRegistry::getStandardExtensions(), &descriptor.info, &fake.factory, 8192);
    std::vector<std::vector<uint8_t>> blocks;
    void* descriptorData = nullptr;
    check(instance.setupAAPXSInstances([&](const char* uri, auto* serialization) {
        blocks.emplace_back(serialization->data_capacity);
        serialization->data = blocks.back().data();
        if (!strcmp(uri, AAPXS_TRANSPORT_URI)) descriptorData = serialization->data;
        return true;
    }), "superseded scan setup");
    if (negotiated) {
        __atomic_store_n(static_cast<uint32_t*>(descriptorData) + 3, AAPXS_TRANSPORT_SYSEX8, __ATOMIC_RELEASE);
        instance.getAAPXSDispatcher().refreshTransport();
    }
    instance.setInstanceId(5); instance.completeInstantiation(); instance.setupTestBuffer();
    fake.metadataShared = instance.getAAPXSDispatcher().getPluginAAPXSByUri(AAP_PARAMETERS_EXTENSION_URI)->serialization;
    std::promise<void> held;
    aapxs_completion_callback oldCompletion = nullptr;
    aapxs_error_callback oldError = nullptr;
    void* oldContext = nullptr;
    std::atomic<bool> newer{false}, boundaryObserved{false};
    std::atomic<int> changes{0};
    fake.metadataHandler = [&](int32_t size, int32_t requestId, int32_t opcode, aapxs_completion_callback callback,
                              void* context, aapxs_error_callback errorCallback) {
        check(!RealtimeScope::isActive(), "superseded metadata stays off DSP");
        ++fake.metadataRequests;
        auto shared = fake.metadataShared;
        int32_t index{}; if (size) memcpy(&index, shared->data, sizeof(index));
        aap_parameter_info_t parameter{};
        aap_parameter_enum_t enumeration{};
        int32_t count = 2;
        const void* reply = &count;
        size_t replySize = sizeof(count);
        auto publishReply = [&] {
            memcpy(shared->data, reply, replySize); shared->data_size = replySize;
            if (!negotiated) {
                uint32_t ump[512]{}; uint8_t conversion[2048]{};
                auto length = aap_midi2_generate_aapxs_sysex8(ump, 512, conversion, sizeof(conversion),
                    0, requestId, 0, AAP_PARAMETERS_EXTENSION_URI, opcode,
                    static_cast<const uint8_t*>(reply), replySize);
                check(length && fake.legacyReplies.tryPush(ump, length), "superseded legacy replies stay bounded");
            }
        };
        switch (opcode) {
            case OPCODE_PARAMETERS_GET_PARAMETER_COUNT:
                count = 80; break;
            case OPCODE_PARAMETERS_GET_PARAMETER:
                parameter = {static_cast<int16_t>(index), "current-layout", "", 0, 100, 30, false};
                reply = &parameter; replySize = sizeof(parameter);
                if (!newer && index == 0) {
                    strcpy(parameter.display_name, "obsolete-layout");
                    publishReply();
                    oldCompletion = callback; oldError = errorCallback; oldContext = context;
                    held.set_value();
                    return true; // retain the actual transport request until released
                }
                break;
            case OPCODE_PARAMETERS_GET_ENUMERATION_COUNT: break;
            case OPCODE_PARAMETERS_GET_ENUMERATION:
                memcpy(&index, static_cast<uint8_t*>(shared->data) + 4, 4);
                enumeration.value = index; strcpy(enumeration.name, "current-enum");
                reply = &enumeration; replySize = sizeof(enumeration); break;
            default: check(false, "unexpected superseded scan opcode");
        }
        publishReply();
        callback(context, &fake.api);
        return true;
    };
    instance.setIpcExtensionMessageSender([](void* context, const char*, int32_t, int32_t size,
            int32_t requestId, int32_t opcode, aapxs_completion_callback callback, void* callbackContext, aapxs_error_callback errorCallback) {
        return static_cast<FakePlugin*>(context)->metadataHandler(size, requestId, opcode, callback, callbackContext, errorCallback);
    });
    auto initialSnapshot = internal::getParameterMetadataSnapshot(instance);
    std::atomic<int> legacyNotifications{0}, metadataNotifications{0};
    internal::setParameterLayoutChangedListener(instance, [&] { ++legacyNotifications; });
    internal::setParameterMetadataChangedListener(instance, [&] {
        auto snapshot = internal::getParameterMetadataSnapshot(instance);
        check(snapshot.revision > initialSnapshot.revision && snapshot.parameters.size() == 80 &&
              !strcmp(snapshot.parameters.front().getName(), "current-layout"),
              "bulk metadata snapshot pairs a new revision with a complete current layout");
        ++metadataNotifications;
    });
    instance.parametersChangedHandler = [&](auto& remote) {
        check(remote.getNumParameters() == 80 && !strcmp(remote.getParameter(0)->getName(), "current-layout"),
              "obsolete partial layout is never published");
        ++changes;
    };
    instance.beforeWorkerWait = [&] {
        if (newer && instance.getRealtimeState().layout_refresh.load()) boundaryObserved = true;
    };
    auto driveUntil = [&](auto predicate) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (!predicate() && std::chrono::steady_clock::now() < deadline) {
            instance.process(64, 0); std::this_thread::yield();
        }
        check(predicate(), "superseded scan makes progress");
    };
    internal::requestParameterLayoutRefresh(instance);
    auto heldResult = held.get_future();
    driveUntil([&] { return heldResult.wait_for(std::chrono::seconds(0)) == std::future_status::ready; });
    newer = true;
    // A burst must not queue abandoned replacement requests behind the held read.
    for (int n = 0; n < 20; ++n) internal::requestParameterLayoutRefresh(instance);
    driveUntil([&] { return boundaryObserved.load(); });
    check(fake.metadataRequests == 2 && changes == 0, "supersession waits only for the outstanding read");
    if (failedOldReply) oldError(oldContext, &fake.api, "obsolete reply failed");
    else oldCompletion(oldContext, &fake.api);
    driveUntil([&] { return changes.load() != 0; });
    instance.stopExtensionWorker();
    check(legacyNotifications == 1 && metadataNotifications == 1,
          "public C++ handler, legacy Kotlin listener and snapshot publisher coexist");
    check(changes == 1 && fake.metadataRequests == (negotiated ? 323 : 643),
          "skip the obsolete scan tail and publish only one complete replacement");
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
    guardProbes(); sharedObjectScope(); sharedTransportNegotiation(); deferredMidiBuffers(); localProcessing(); countPolling(); extensionNeutralDispatch(); layoutReadiness(); remoteProcessing(); activeLayoutRefresh(false); activeLayoutRefresh(true);
    supersededLayoutRefresh(true, false); supersededLayoutRefresh(true, true);
    supersededLayoutRefresh(false, false); supersededLayoutRefresh(false, true);
    incomingControlAndTeardown(false); incomingControlAndTeardown(true);
    deferredRecipientReply();
    puts("PASS: actual local/remote processing, cached proxies, notifications and suspension without locks or C++ allocation");
}
