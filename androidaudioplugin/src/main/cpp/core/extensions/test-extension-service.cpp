
#include "aap/core/host/extension-service.h"

// Extension API --------------------------------------

const char * AAPXS_EXAMPLE_TEST_EXTENSION_URI = "urn:aapxs/example/test/feature";

// we have to resort to this forward declaration in the function typedefs below.
struct example_test_extension_t;

// Extension function #1: takes an integer input and do something, return an integer value.
//  Context may be required in the applied plugins (depends on each plugin developer).
typedef int32_t (*fooFunc) (struct example_test_extension_t* context, int32_t input);

// Extension function #2: takes a string input and do something. Nothing to return.
//  Context may be required in the applied plugins (depends on each plugin developer).
typedef void (*barFunc) (struct example_test_extension_t* context, char *msg);

// The extension structure
typedef struct example_test_extension_t {
    void *context; // it might be AndroidAudioPlugin, or AAPXSClientInstance
    fooFunc foo;
    barFunc bar;
} example_test_extension_t;

// Extension Service ----------------------------------

const int32_t AAPXS_EXAMPLE_TEST_OPCODE_FOO = 0;
const int32_t AAPXS_EXAMPLE_TEST_OPCODE_BAR = 1;

// FIXME: allocate this per extension service client instance
char instance_message_buffer[1024];

void test_extension_feature_on_invoked(
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

int32_t proxy_foo (example_test_extension_t* plugin, int32_t input) {
    // FIXME: implement
    assert(false);
    return 0;
}

void proxy_bar (example_test_extension_t* plugin, char *msg) {
    // FIXME: implement
    assert(false);
}


void* test_extension_feature_as_proxy(AAPXSClientInstance *extension) {
    return &proxy;
}

AAPXSFeature test_extensions_feature{AAPXS_EXAMPLE_TEST_EXTENSION_URI,
                                     test_extension_feature_on_invoked,
                                     test_extension_feature_as_proxy};
