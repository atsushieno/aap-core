#include "AapJsControllerRuntime.h"

// The heavy amalgamated QuickJS build is pulled in here, in this single translation unit only.
#include <choc/javascript/choc_javascript_QuickJS.h>
#include <choc/text/choc_JSON.h>

#include <aap/core/host/plugin-host.h>
#include <aap/core/host/plugin-instance.h>
#include <aap/core/plugin-information.h>
#include <aap/ext/port-config.h>
#include "plugin-parameter-state.h"

#include <android/log.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
#include <vector>

#define LOG_TAG "aap-js-controller"

namespace aap::js {

namespace {

// Minimal RFC 4648 base64, used to ferry opaque plugin state to/from JavaScript as strings.
const char* kBase64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = data[i] << 16;
        if (i + 1 < len) n |= data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];
        out.push_back(kBase64Chars[(n >> 18) & 0x3F]);
        out.push_back(kBase64Chars[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? kBase64Chars[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? kBase64Chars[n & 0x3F] : '=');
    }
    return out;
}

std::vector<uint8_t> base64Decode(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::vector<uint8_t> out;
    int buffer = 0, bits = 0;
    for (char c : in) {
        int v = val(c);
        if (v < 0) continue; // skip '=' and whitespace
        buffer = (buffer << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t) ((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

std::vector<uint8_t> hexDecode(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<uint8_t> out;
    int high = -1;
    for (char c : in) {
        int v = val(c);
        if (v < 0) continue; // skip whitespace/separators
        if (high < 0)
            high = v;
        else {
            out.push_back(static_cast<uint8_t>((high << 4) | v));
            high = -1;
        }
    }
    if (high >= 0)
        throw std::runtime_error("odd-length hex UMP input");
    return out;
}

} // namespace

AapJsControllerRuntime& AapJsControllerRuntime::global() {
    static AapJsControllerRuntime instance;
    return instance;
}

void AapJsControllerRuntime::ensureContext() {
    if (!context_) {
        context_ = std::make_unique<choc::javascript::Context>(choc::javascript::createQuickJSContext());
        registerBindings();
    }
}

aap::PluginClient* AapJsControllerRuntime::requireClient() const {
    if (!client_)
        throw std::runtime_error("No AAP client is attached. Call AapAutomationRuntime.attachNativeClient() first.");
    return client_;
}

void AapJsControllerRuntime::bootstrap(const std::string& apiJsSource) {
    std::lock_guard lock(mutex_);
    ensureContext();
    if (bootstrapped_)
        return;
    apiJsSource_ = apiJsSource;
    context_->evaluateExpression(apiJsSource_);
    bootstrapped_ = true;
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "aap-api.js bootstrapped (%zu bytes)", apiJsSource.size());
}

void AapJsControllerRuntime::setClient(int64_t nativeClientHandle) {
    std::lock_guard lock(mutex_);
    client_ = reinterpret_cast<aap::PluginClient*>(nativeClientHandle);
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "client attached: %p", (void*) client_);
}

void AapJsControllerRuntime::setPluginCatalog(const std::string& json) {
    std::lock_guard lock(mutex_);
    pluginCatalogJson_ = json.empty() ? "[]" : json;
}

std::string AapJsControllerRuntime::runScript(const std::string& code) {
    std::lock_guard lock(mutex_);
    try {
        ensureContext();
        auto result = context_->evaluateExpression(code);
        return result.isVoid() ? std::string("undefined") : choc::json::toString(result);
    } catch (const std::exception& e) {
        return std::string("ERROR: ") + e.what();
    }
}

std::string AapJsControllerRuntime::startJob(const std::string& code) {
    std::shared_ptr<AutomationJob> job;
    {
        std::lock_guard lock(jobsMutex_);
        job = std::make_shared<AutomationJob>();
        job->jobId = "job-" + std::to_string(nextJobId_++);
        jobs_[job->jobId] = job;
    }
    std::thread([this, job, code]() {
        std::string result = runScript(code);
        std::lock_guard lock(jobsMutex_);
        job->state = result.rfind("ERROR:", 0) == 0 ? "failed" : "completed";
        // result is already JSON (or an ERROR string); wrap the latter as a JSON string.
        if (job->state == "failed")
            job->result = choc::json::toString(choc::value::Value(result));
        else
            job->result = result;
    }).detach();
    return job->jobId;
}

std::string AapJsControllerRuntime::getJob(const std::string& jobId) {
    std::lock_guard lock(jobsMutex_);
    auto it = jobs_.find(jobId);
    if (it == jobs_.end())
        return "{\"error\":\"job not found\"}";
    auto& job = *it->second;
    auto obj = choc::value::createObject("");
    obj.setMember("jobId", job.jobId);
    obj.setMember("state", job.state);
    // result is raw JSON; embed by parsing so it nests as a value rather than a string.
    try {
        obj.setMember("result", choc::json::parse(job.result));
    } catch (...) {
        obj.setMember("result", job.result);
    }
    return choc::json::toString(obj);
}

void AapJsControllerRuntime::clearJob(const std::string& jobId) {
    std::lock_guard lock(jobsMutex_);
    jobs_.erase(jobId);
}

void AapJsControllerRuntime::registerBindings() {
    using namespace choc::value;
    auto& ctx = *context_;

    ctx.registerFunction("__aap_ping", [](choc::javascript::ArgumentList) -> Value {
        return Value("pong");
    });

    ctx.registerFunction("__aap_runtime_info", [this](choc::javascript::ArgumentList) -> Value {
        auto obj = createObject("");
        obj.setMember("attached", client_ != nullptr);
        return obj;
    });

    ctx.registerFunction("__aap_sleep_ms", [](choc::javascript::ArgumentList args) -> Value {
        auto milliseconds = args.get<int64_t>(0);
        if (milliseconds > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        return {};
    });

    ctx.registerFunction("__aap_get_plugins", [this](choc::javascript::ArgumentList) -> Value {
        // Returned as a JSON string; the JS facade calls JSON.parse on it.
        return Value(pluginCatalogJson_);
    });

    // Bind a plugin's Android service by package name (JVM upcall). Must happen before
    // instancing; the JS facade calls this from aap.instancing.connect / auto-connect in create.
    ctx.registerFunction("__aap_connect_service", [](choc::javascript::ArgumentList args) -> Value {
        return Value(aap::js::jvmConnectService(args.get<std::string>(0)));
    });

    ctx.registerFunction("__aap_instance_create", [this](choc::javascript::ArgumentList args) -> Value {
        auto client = requireClient();
        auto pluginId = args.get<std::string>(0);
        auto result = client->createInstance(pluginId, true);
        if (!result.error.empty())
            throw std::runtime_error("createInstance failed: " + result.error);
        return Value((int64_t) result.value);
    });

    ctx.registerFunction("__aap_instance_destroy", [this](choc::javascript::ArgumentList args) -> Value {
        auto client = requireClient();
        auto instance = client->getInstanceById((int32_t) args.get<int64_t>(0));
        if (instance)
            client->destroyInstance(instance);
        return {};
    });

    ctx.registerFunction("__aap_instance_prepare", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        instance->prepare((int) args.get<int64_t>(1), (int32_t) args.get<int64_t>(2));
        return {};
    });

    ctx.registerFunction("__aap_instance_activate", [this](choc::javascript::ArgumentList args) -> Value {
        requireClient()->getInstanceById((int32_t) args.get<int64_t>(0))->activate();
        return {};
    });

    ctx.registerFunction("__aap_instance_deactivate", [this](choc::javascript::ArgumentList args) -> Value {
        requireClient()->getInstanceById((int32_t) args.get<int64_t>(0))->deactivate();
        return {};
    });

    ctx.registerFunction("__aap_instance_process", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto frameCount = (int32_t) args.get<int64_t>(1);
        // 1s timeout; this is a single non-realtime block (the offline renderer comes later).
        instance->process(frameCount, 1000000000L);
        return {};
    });

    ctx.registerFunction("__aap_instance_fill_audio_inputs", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto seed = (int32_t) args.get<int64_t>(1);
        auto amplitude = args.get<double>(2);
        auto* buffer = instance->getAudioPluginBuffer();
        if (!buffer)
            return {};
        auto frames = buffer->num_frames(buffer);
        for (int32_t p = 0, n = instance->getNumPorts(); p < n; ++p) {
            auto* port = instance->getPort(p);
            if (!port || port->getContentType() != AAP_CONTENT_TYPE_AUDIO ||
                port->getPortDirection() != AAP_PORT_DIRECTION_INPUT)
                continue;
            auto* data = static_cast<float*>(buffer->get_buffer(buffer, p));
            if (!data)
                continue;
            auto samples = std::min<int32_t>(frames, buffer->get_buffer_size(buffer, p) / sizeof(float));
            for (int32_t i = 0; i < samples; ++i) {
                auto phase = static_cast<double>((seed + i + p * 31) % 97) / 97.0;
                data[i] = static_cast<float>((std::sin(phase * 6.283185307179586) * 0.7 +
                                              std::sin(phase * 18.84955592153876) * 0.3) * amplitude);
            }
        }
        return {};
    });

    // Offline test-graph edge: copy every audio output of source into the corresponding audio
    // input of destination. Ports are paired by their audio-port order, rather than absolute port
    // number, because MIDI and parameter ports may be interleaved with audio ports.
    ctx.registerFunction("__aap_instance_copy_audio_outputs_to_inputs", [this](choc::javascript::ArgumentList args) -> Value {
        auto source = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto destination = requireClient()->getInstanceById((int32_t) args.get<int64_t>(1));
        auto* sourceBuffer = source->getAudioPluginBuffer();
        auto* destinationBuffer = destination->getAudioPluginBuffer();
        if (!sourceBuffer || !destinationBuffer)
            return {};

        std::vector<int32_t> sourcePorts;
        std::vector<int32_t> destinationPorts;
        for (int32_t p = 0, n = source->getNumPorts(); p < n; ++p) {
            auto* port = source->getPort(p);
            if (port && port->getContentType() == AAP_CONTENT_TYPE_AUDIO &&
                port->getPortDirection() == AAP_PORT_DIRECTION_OUTPUT)
                sourcePorts.push_back(p);
        }
        for (int32_t p = 0, n = destination->getNumPorts(); p < n; ++p) {
            auto* port = destination->getPort(p);
            if (port && port->getContentType() == AAP_CONTENT_TYPE_AUDIO &&
                port->getPortDirection() == AAP_PORT_DIRECTION_INPUT)
                destinationPorts.push_back(p);
        }

        const auto portCount = std::min(sourcePorts.size(), destinationPorts.size());
        for (size_t i = 0; i < portCount; ++i) {
            auto* from = static_cast<float*>(sourceBuffer->get_buffer(sourceBuffer, sourcePorts[i]));
            auto* to = static_cast<float*>(destinationBuffer->get_buffer(destinationBuffer, destinationPorts[i]));
            if (!from || !to)
                continue;
            const auto samples = std::min({
                sourceBuffer->num_frames(sourceBuffer),
                destinationBuffer->num_frames(destinationBuffer),
                sourceBuffer->get_buffer_size(sourceBuffer, sourcePorts[i]) / (int32_t) sizeof(float),
                destinationBuffer->get_buffer_size(destinationBuffer, destinationPorts[i]) / (int32_t) sizeof(float)
            });
            std::copy_n(from, std::max(0, samples), to);
        }
        return {};
    });

    ctx.registerFunction("__aap_instance_get_audio_output_stats", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto* buffer = instance->getAudioPluginBuffer();
        auto arr = createEmptyArray();
        if (!buffer)
            return arr;
        for (int32_t p = 0, n = instance->getNumPorts(); p < n; ++p) {
            auto* port = instance->getPort(p);
            if (!port || port->getContentType() != AAP_CONTENT_TYPE_AUDIO ||
                port->getPortDirection() != AAP_PORT_DIRECTION_OUTPUT)
                continue;
            auto* data = static_cast<float*>(buffer->get_buffer(buffer, p));
            auto samples = data ? std::min<int32_t>(buffer->num_frames(buffer), buffer->get_buffer_size(buffer, p) / sizeof(float)) : 0;
            double sum = 0.0, sumAbs = 0.0, sumSq = 0.0, maxAbs = 0.0;
            uint32_t hash = 2166136261u;
            for (int32_t i = 0; i < samples; ++i) {
                auto v = static_cast<double>(data[i]);
                auto av = std::abs(v);
                sum += v;
                sumAbs += av;
                sumSq += v * v;
                maxAbs = std::max(maxAbs, av);
                int32_t q = static_cast<int32_t>(std::max(-1.0, std::min(1.0, v)) * 2147483647.0);
                hash ^= static_cast<uint32_t>(q);
                hash *= 16777619u;
            }
            auto obj = createObject("");
            obj.setMember("port", static_cast<int64_t>(p));
            obj.setMember("samples", static_cast<int64_t>(samples));
            obj.setMember("sum", sum);
            obj.setMember("sumAbs", sumAbs);
            obj.setMember("rms", samples > 0 ? std::sqrt(sumSq / samples) : 0.0);
            obj.setMember("maxAbs", maxAbs);
            obj.setMember("hash", static_cast<int64_t>(hash));
            arr.addArrayElement(obj);
        }
        return arr;
    });

    ctx.registerFunction("__aap_instance_add_event_ump_input_hex", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto bytes = hexDecode(args.get<std::string>(1));
        if (!bytes.empty())
            instance->addEventUmpInput(bytes.data(), static_cast<int32_t>(bytes.size()));
        return {};
    });

    ctx.registerFunction("__aap_instance_get_parameter_count", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        return Value((int64_t) instance->getNumParameters());
    });

    ctx.registerFunction("__aap_instance_get_parameters", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto arr = createEmptyArray();
        for (int32_t i = 0, n = instance->getNumParameters(); i < n; i++) {
            auto* p = instance->getParameter(i);
            auto obj = createObject("");
            obj.setMember("index", (int64_t) i);
            if (p) {
                obj.setMember("id", (int64_t) p->getId());
                obj.setMember("name", std::string(p->getName()));
                obj.setMember("minValue", p->getMinimumValue());
                obj.setMember("maxValue", p->getMaximumValue());
                obj.setMember("defaultValue", p->getDefaultValue());
            }
            arr.addArrayElement(obj);
        }
        return arr;
    });

    ctx.registerFunction("__aap_instance_get_parameter_value", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        return Value(aap::internal::getParameterValue(*instance, (int32_t) args.get<int64_t>(1)));
    });

    ctx.registerFunction("__aap_instance_get_preset_count", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        return Value((int64_t) instance->getStandardExtensions().getPresetCount());
    });

    ctx.registerFunction("__aap_instance_get_preset_name", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        return Value(instance->getStandardExtensions().getPresetName((int32_t) args.get<int64_t>(1)));
    });

    ctx.registerFunction("__aap_instance_set_preset", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        instance->getStandardExtensions().setCurrentPresetIndex((int32_t) args.get<int64_t>(1));
        return {};
    });

    ctx.registerFunction("__aap_instance_get_state", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto& ext = instance->getStandardExtensions();
        if (ext.getStateSize() <= 0)
            return Value(std::string());
        auto state = ext.getState().value;
        return Value(base64Encode(static_cast<const uint8_t*>(state.data), state.data_size));
    });

    ctx.registerFunction("__aap_instance_set_state", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        auto bytes = base64Decode(args.get<std::string>(1));
        instance->getStandardExtensions().setState(bytes.data(), bytes.size());
        return {};
    });

    ctx.registerFunction("__aap_instance_create_gui", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        return Value((int64_t) instance->getStandardExtensions().createGui(
                instance->getPluginInformation()->getPluginID(),
                instance->getInstanceId(),
                nullptr));
    });

    ctx.registerFunction("__aap_instance_show_gui", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        instance->getStandardExtensions().showGui((int32_t) args.get<int64_t>(1));
        return {};
    });

    ctx.registerFunction("__aap_instance_hide_gui", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        instance->getStandardExtensions().hideGui((int32_t) args.get<int64_t>(1));
        return {};
    });

    ctx.registerFunction("__aap_instance_destroy_gui", [this](choc::javascript::ArgumentList args) -> Value {
        auto instance = requireClient()->getInstanceById((int32_t) args.get<int64_t>(0));
        instance->getStandardExtensions().destroyGui((int32_t) args.get<int64_t>(1));
        return {};
    });
}

} // namespace aap::js
