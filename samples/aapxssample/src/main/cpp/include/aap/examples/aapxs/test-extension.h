// Extension API --------------------------------------

const char * AAPXS_EXAMPLE_TEST_EXTENSION_URI = "urn://androidaudioplugin.org/examples/aapxs/test1/v1";

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

