/*
 * It is an example plugin that provides full extension service implementation *without* C++
 * support features in libandroidaudioplugin. It is to verify the public AAPXS API.
 *
 */

#include "aap/core/aapxs/typed-aapxs.h"
#include "aap/examples/aapxs/test-extension.h"

// Extension Service ----------------------------------

const int32_t AAPXS_EXAMPLE_TEST_OPCODE_FOO = 0;
const int32_t AAPXS_EXAMPLE_TEST_OPCODE_BAR = 1;

const int32_t AAPXS_EXAMPLE_TEST_MAX_MESSAGE_SIZE = 4096;

// FIXME: allocate this per extension service client instance
char instance_message_buffer[1024];

// client proxy

example_test_extension_t proxy{};

void aapxs_example_process_incoming_plugin_aapxs_request(
        struct AAPXSDefinition* definition,
        AAPXSRecipientInstance* aapxsInstance,
        AndroidAudioPlugin* plugin,
        AAPXSRequestContext* request) {

    auto ext = (example_test_extension_t *) request->serialization->data;

    switch (request->opcode) {
        case AAPXS_EXAMPLE_TEST_OPCODE_FOO: {
            int32_t input = *(int32_t *) request->serialization->data;
            int32_t ret = ext->foo(nullptr, plugin, input);
            *(int32_t *) request->serialization->data = ret;
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
        case AAPXS_EXAMPLE_TEST_OPCODE_BAR: {
            int32_t size = *(int32_t*) request->serialization->data;
            strncpy(instance_message_buffer, const_cast<char*>((char*) request->serialization->data + sizeof(int32_t)), size);
            ext->bar(nullptr, plugin, instance_message_buffer);
            aapxsInstance->send_aapxs_reply(aapxsInstance, request);
            break;
        }
    }
}

void aapxs_example_process_incoming_host_aapxs_request(
        struct AAPXSDefinition* definition,
        AAPXSRecipientInstance* aapxsInstance,
        AndroidAudioPluginHost* host,
        AAPXSRequestContext* request) {
    // there is no host extension!
    throw std::runtime_error("should not happen");
}

void aapxs_example_process_incoming_plugin_aapxs_reply(
        struct AAPXSDefinition* definition,
        AAPXSInitiatorInstance* aapxsInstance,
        AndroidAudioPlugin* plugin,
        AAPXSRequestContext* request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, plugin, request->request_id);
}

void aapxs_example_process_incoming_host_aapxs_reply(
        struct AAPXSDefinition* definition,
        AAPXSInitiatorInstance* aapxsInstance,
        AndroidAudioPluginHost* host,
        AAPXSRequestContext* request) {
    if (request->callback != nullptr)
        request->callback(request->callback_user_data, host, request->request_id);
}

namespace aap::xs {
    class AAPXSDefinition_Example : public AAPXSDefinitionWrapper {

        AAPXSDefinition aapxs_example{AAPXS_EXAMPLE_TEST_EXTENSION_URI,
                                      this,
                                      AAPXS_EXAMPLE_TEST_MAX_MESSAGE_SIZE + sizeof(int32_t),
                                      aapxs_example_process_incoming_plugin_aapxs_request,
                                      aapxs_example_process_incoming_host_aapxs_request,
                                      aapxs_example_process_incoming_plugin_aapxs_reply,
                                      aapxs_example_process_incoming_host_aapxs_reply
        };

    public:
        AAPXSDefinition& asPublic() override {
            return aapxs_example;
        }
    };

    class ExampleClientAAPXS : public TypedAAPXS {
    public:
        ExampleClientAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAPXS_EXAMPLE_TEST_EXTENSION_URI, initiatorInstance, serialization) {
        }

        int32_t foo(int32_t input);
        void bar(const char* msg);
    };

    int32_t ExampleClientAAPXS::foo(int32_t input) {
        *((int32_t*)serialization->data) = input;
        return callTypedFunctionSynchronously<int32_t>(AAPXS_EXAMPLE_TEST_OPCODE_FOO);
    }

    void ExampleClientAAPXS::bar(const char *msg) {
        auto len = strlen(msg);
        *((int32_t*)serialization->data) = len;
        strncpy((char*) serialization->data + sizeof(int32_t), msg, len);
        callVoidFunctionSynchronously(AAPXS_EXAMPLE_TEST_OPCODE_BAR);
    }

    class StateServiceAAPXS : public TypedAAPXS {
    public:
        StateServiceAAPXS(AAPXSInitiatorInstance* initiatorInstance, AAPXSSerializationContext* serialization)
                : TypedAAPXS(AAPXS_EXAMPLE_TEST_EXTENSION_URI, initiatorInstance, serialization) {
        }
        // nothing
    };
}

