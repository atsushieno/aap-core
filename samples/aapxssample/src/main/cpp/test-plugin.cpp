
#include <memory>
#include "aap/android-audio-plugin.h"
#include "aap/examples/aapxs/test-extension.h"

void test_plugin_prepare(AndroidAudioPlugin *plugin, aap_buffer_t *buffer) {}

void test_plugin_activate(AndroidAudioPlugin *plugin) {}

void test_plugin_process(AndroidAudioPlugin *plugin, aap_buffer_t *buffer, int32_t frameCount, int64_t timeoutInNanoiseconds) {}

void test_plugin_deactivate(AndroidAudioPlugin *plugin) {}

int32_t test_foo (struct example_test_extension_t* context, int32_t input) { return 0; }

void test_bar (struct example_test_extension_t* context, char* msg) {}

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
            nullptr,
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
    delete instance;
}

AndroidAudioPluginFactory factory{test_plugin_new, test_plugin_delete};

AndroidAudioPluginFactory *GetAndroidAudioPluginFactory() {
    return &factory;
}
