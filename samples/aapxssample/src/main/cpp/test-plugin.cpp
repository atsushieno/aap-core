
#include <cassert>
#include <memory>
#include "aap/android-audio-plugin.h"
#include "aap/examples/aapxs/test-extension.h"
#include "aap/ext/parameters.h"

struct AAPXSSampleContext {
    AndroidAudioPluginHost* host;
    aap_parameters_host_extension_t* params_ext;

    AAPXSSampleContext(AndroidAudioPluginHost* host) :
            host(host) {
        params_ext = (aap_parameters_host_extension_t*) host->get_extension(host, AAP_PARAMETERS_EXTENSION_URI);
        assert(params_ext);
    }
};

void test_plugin_prepare(AndroidAudioPlugin *plugin, aap_buffer_t *buffer) {}

void test_plugin_activate(AndroidAudioPlugin *plugin) {}

void test_plugin_process(AndroidAudioPlugin *plugin, aap_buffer_t *buffer, int32_t frameCount, int64_t timeoutInNanoiseconds) {
    auto context = (AAPXSSampleContext*) plugin->plugin_specific;
    auto params = context->params_ext;
    params->notify_parameters_changed(context->params_ext, context->host);
    if (frameCount == INT32_MAX) throw std::runtime_error("whoa"); // NOP in practice, just to make sure we successfully pass host extension calls.
}

void test_plugin_deactivate(AndroidAudioPlugin *plugin) {}

int32_t test_foo (struct example_test_extension_t* context, AndroidAudioPlugin* plugin, int32_t input) { return 0; }

void test_bar (struct example_test_extension_t* context, AndroidAudioPlugin* plugin, char* msg) {}

example_test_extension_t test_ext{nullptr, test_foo, test_bar};

void* test_plugin_get_extension(AndroidAudioPlugin *plugin, const char* uri) {
    if (!strcmp(uri, AAPXS_EXAMPLE_TEST_EXTENSION_URI))
        return &test_ext;
    return nullptr;
}

AndroidAudioPlugin *test_plugin_new(
        AndroidAudioPluginFactory *pluginFactory,
        const char *pluginUniqueId,
        int sampleRate,
        AndroidAudioPluginHost *host) {
    return new AndroidAudioPlugin{
            new AAPXSSampleContext(host),
            test_plugin_prepare,
            test_plugin_activate,
            test_plugin_process,
            test_plugin_deactivate,
            test_plugin_get_extension
    };
}

void test_plugin_delete(
        AndroidAudioPluginFactory *pluginFactory,
        AndroidAudioPlugin *instance) {
    delete (AAPXSSampleContext*) instance->plugin_specific;
    delete instance;
}

AndroidAudioPluginFactory factory{test_plugin_new, test_plugin_delete};

AndroidAudioPluginFactory *GetAndroidAudioPluginFactory() {
    return &factory;
}
