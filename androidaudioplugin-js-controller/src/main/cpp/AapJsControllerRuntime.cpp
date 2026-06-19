#include "AapJsControllerRuntime.h"

// The heavy amalgamated QuickJS build is pulled in here, in this single translation unit only.
#include <choc/javascript/choc_javascript_QuickJS.h>
#include <choc/text/choc_JSON.h>

#include <aap/core/host/plugin-host.h>
#include <aap/core/host/plugin-instance.h>
#include <aap/core/plugin-information.h>

#include <android/log.h>
#include <chrono>
#include <stdexcept>
#include <thread>

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

    ctx.registerFunction("__aap_get_plugins", [this](choc::javascript::ArgumentList) -> Value {
        // Returned as a JSON string; the JS facade calls JSON.parse on it.
        return Value(pluginCatalogJson_);
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
        return Value(instance->getParameterValue((int32_t) args.get<int64_t>(1)));
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
}

} // namespace aap::js
