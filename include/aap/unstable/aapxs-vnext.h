#ifndef AAP_CORE_AAPXS_VNEXT_H
#define AAP_CORE_AAPXS_VNEXT_H

// The new 2023 version of AAPXS runtime - ABI compatibility.

#include <cstdint>
#include "aap/android-audio-plugin.h"

typedef struct AAPXSSerializationContext {
    void* data;
    size_t data_size;
    size_t data_capacity;
} AAPXSSerializationContext;

typedef struct AAPXSRequestContext {
    aapxs_completion_callback callback;
    void* callback_user_data;
    AAPXSSerializationContext* serialization;
    const char * uri;
    uint32_t request_id;
    int32_t opcode;
} AAPXSRequestContext;

// Stores audio processing shared memory buffers (could be used without binder)
typedef struct AAPXSProxyContextVNext {
    const char* uri;
    void* extension;
    AAPXSSerializationContext serialization;
} AAPXSProxyContextVNext;

// client instance for plugin extension API, and service instance for host extension API
typedef struct AAPXSInitiatorInstance {
    void* host_context;
    AAPXSSerializationContext* serialization;

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer, for async implementation
    // - FIXME: this may become framework reference implementation
    uint32_t (*get_new_request_id) (AAPXSInitiatorInstance* instance);

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer
    // It is supposed to keep context until the assigner received a corresponding reply.
    void (*send_aapxs_request) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);

    // assigned by: AAPXS developer
    // invoked by: framework reference implementation
    void (*process_incoming_aapxs_reply) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);
} AAPXSInitiatorInstance;

// service instance for plugin extension API, and client instance for host extension API
typedef struct AAPXSRecipientInstance {
    void* host_context;
    AAPXSSerializationContext* serialization;

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer, for async implementation
    // - FIXME: this may become framework reference implementation
    uint32_t (*get_new_request_id) (AAPXSRecipientInstance* instance);

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer
    void (*process_incoming_aapxs_request) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer
    void (*send_aapxs_reply) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);
} AAPXSRecipientInstance;

/**
 * The "untyped" AAPXS definition (in the public API surface).
 * Each AAPXS needs to provide an instance of this type so that host framework (reference
 * implementation) can register at its AAPXS map.
 *
 * This type provides a handful of handler functions to deal with AAPXS requests and replies, for
 * both the "plugin extension" and the "host extension".
 * They are implemented and assigned by each AAPXS implementation, and invoked by the host framework
 * (reference implementation) that would delegate to each strongly-typed AAPXS function (which is
 * hidden behind `aapxs_context` opaque pointer).
 */
typedef struct AAPXSDefinition {
    /** The extension URI */
    const char *uri;

    /** An opaque pointer to the AAPXS developers' context.
     * It should be assigned and used only by the AAPXS developers of the extension.
     */
    void *aapxs_context;

    /**
     * The data capacity that is used to allocate shared memory for the binary transfer storage (Binder/SysEx8).
     */
    int32_t data_capacity;

    /** Invoked when a plugin extension request has arrived at the service.
     * Assigned by the host framework (reference implementation).
     * Invoked by each AAPXS implementation.
     *
     * The parameter `definition` provides access to aapxs_context to help AAPXS developers encapsulate the implementation details.
     *
     * @param definition The containing AAPXSDefinition.
     * @param aapxsInstance
     * @param plugin The target plugin
     * @param context The request context that provides access to data, opcode, request ID, etc.
     */
    void (*process_incoming_plugin_aapxs_request) (
            struct AAPXSDefinition* definition,
            AAPXSRecipientInstance* aapxsInstance,
            AndroidAudioPlugin* plugin,
            AAPXSRequestContext* context);
    void (*process_incoming_host_aapxs_request) (
            struct AAPXSDefinition* definition,
            AAPXSRecipientInstance* aapxsInstance,
            AndroidAudioPluginHost* host,
            AAPXSRequestContext* context);
    void (*process_incoming_plugin_aapxs_reply) (
            struct AAPXSDefinition* definition,
            AAPXSInitiatorInstance* aapxsInstance,
            AndroidAudioPlugin* plugin,
            AAPXSRequestContext* context);
    void (*process_incoming_host_aapxs_reply) (
            struct AAPXSDefinition* definition,
            AAPXSInitiatorInstance* aapxsInstance,
            AndroidAudioPluginHost* host,
            AAPXSRequestContext* context);
} AAPXSFeatureVNext;

#if 0
typedef struct AAPXSFeatureVNext_tmp {
    /** The extension URI. */
    const char *uri;

    /**
     * AAPXS implementor's context. Only AAPXS developer should assign it and reference it.
     * It should be host implementation agnostic (e.g. no reference to aap::RemotePluginInstance)
     */
    void *context;

    /** The size of required shared memory that we allocate and use between host client and plugin service. */
    int32_t shared_memory_size;

    /**
     * Implemented by the extension developer.
     * Called by AAP framework (service part) to invoke the actual plugin extension.
     * Plugin (service) has direct (in-memory) access to the extension, but arguments are the ones
     * deserialized by the implementation of the extension, not the ones in the host memory space.
     */
    void (*on_invoked) (struct AAPXSFeatureVNext* feature,
                        AndroidAudioPlugin* plugin,
                        AAPXSServiceInstance* extension,
                        int32_t opcode);
    /**
     * Implemented by the extension developer.
     * Called by AAP framework (client part) to return the extension proxy to the extension.
     * Client application has access to the extension service only through the extension API via the proxy.
     */
    AAPXSProxyContextVNext (*as_proxy) (struct AAPXSFeatureVNext* feature, AAPXSClientInstance* extension);

    /** True if get_extension() must return non-null implementation. */
    bool is_implementation_mandatory;
} AAPXSFeatureVNext_tmp;
#endif

#endif //AAP_CORE_AAPXS_VNEXT_H
