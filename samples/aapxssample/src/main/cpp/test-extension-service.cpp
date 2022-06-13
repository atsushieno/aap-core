/*
 * It is an example plugin that provides full extension service implementation *without* C++
 * support features in libandroidaudioplugin. It is to verify the public AAPXS API.
 *
 */

#include "aap/core/host/extension-service.h"
#include "aap/examples/aapxs/test-extension.h"

// Extension Service ----------------------------------

const int32_t AAPXS_EXAMPLE_TEST_OPCODE_FOO = 0;
const int32_t AAPXS_EXAMPLE_TEST_OPCODE_BAR = 1;

const int32_t AAPXS_EXAMPLE_TEST_MAX_MESSAGE_SIZE = 4096;

// FIXME: allocate this per extension service client instance
char instance_message_buffer[1024];

void test_extension_feature_on_invoked(
    AAPXSFeature* feature,
    void* userContext,
    AAPXSServiceInstance *extension,
    int32_t opcode) {

    auto ext = (example_test_extension_t *) extension->data;

    switch (opcode) {
    case AAPXS_EXAMPLE_TEST_OPCODE_FOO: {
        int32_t input = *(int32_t *) extension->data;
        int32_t ret = ext->foo(nullptr, input);
        *(int32_t *) extension->data = ret;
        }
        break;
    case AAPXS_EXAMPLE_TEST_OPCODE_BAR: {
        int32_t size = *(int32_t*) extension->data;
        strncpy(instance_message_buffer, const_cast<char*>((char*)extension->data + sizeof(int32_t)), size);
        ext->bar(nullptr, instance_message_buffer);
        }
        break;
    }
}

// client proxy

example_test_extension_t proxy{};

int32_t proxy_foo (example_test_extension_t* test, int32_t input) {
    // The context is assigned by `test_extension_feature_as_proxy()`.
    auto aapxs = (AAPXSClientInstance*) test->context;
    // set `input` at the very beginning of the shared data pointer.
    *(int32_t*) aapxs->data = input;
    // send the request using AAPXSClient->extension_message().
    aapxs->extension_message(aapxs, AAPXS_EXAMPLE_TEST_OPCODE_FOO);
    // retrieve the return value from the very beginning of the shared data pointer.
    return *(int32_t*) aapxs->data;
}

void proxy_bar (example_test_extension_t* test, char *msg) {
    // The context is assigned by `test_extension_feature_as_proxy()`.
    auto aapxs = (AAPXSClientInstance*) test->context;
    // length-prefixed string: set length at the very beginning of the shared data pointer.
    int32_t len = std::min(AAPXS_EXAMPLE_TEST_MAX_MESSAGE_SIZE, (int32_t) strlen(msg));
    *(int32_t*) aapxs->data = len;
    // then copy the string into the shared buffer
    strncpy((char*) aapxs->data + sizeof(int32_t), msg, len);
    // send the request using AAPXSClient->extension_message().
    aapxs->extension_message(aapxs, AAPXS_EXAMPLE_TEST_OPCODE_BAR);
    // no need to retrieve
}

AAPXSProxyContext test_extension_feature_as_proxy(AAPXSFeature* feature, AAPXSClientInstance *extension) {
    // This `proxy.context` is managed and used by this extension service developer like this, not by anyone else.
    // FIXME: allocate individual example_test_extension_t instance for each plugin instance (or AAPXSClientInstance)
    //proxy.context = extension;
    proxy.foo = proxy_foo;
    proxy.bar = proxy_bar;
    return AAPXSProxyContext {extension, nullptr, &proxy };
}

AAPXSFeature test_extensions_feature{AAPXS_EXAMPLE_TEST_EXTENSION_URI,
                                     nullptr,
                                     AAPXS_EXAMPLE_TEST_MAX_MESSAGE_SIZE + sizeof(int32_t),
                                     test_extension_feature_on_invoked,
                                     test_extension_feature_as_proxy};
