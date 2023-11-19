#ifndef AAP_CORE_AAPXS_H
#define AAP_CORE_AAPXS_H

// The new 2023 version of AAPXS runtime - ABI compatibility.

#define USE_AAPXS_V2 true
#if USE_AAPXS_V2
#define AAPXS_V1_DEPRECATED [[deprecated("Remove use of it")]]
#else
#define AAPXS_V1_DEPRECATED /* none */
#endif

#include <cstdint>
#include "android-audio-plugin.h"

// Created per extension per instance
typedef struct AAPXSSerializationContext {
    void* data;
    size_t data_size;
    size_t data_capacity;
} AAPXSSerializationContext;

typedef struct AAPXSRequestContext {
    aapxs_completion_callback callback;
    void* callback_user_data;
    AAPXSSerializationContext* serialization;
    uint8_t urid;
    const char * uri;
    uint32_t request_id;
    int32_t opcode;
} AAPXSRequestContext;

// client instance for plugin extension API, and service instance for host extension API
typedef struct AAPXSInitiatorInstance {
    // owned by each AAPXS implementation
    void* aapxs_context;
    // owned by hosting implementation
    void* host_context;
    AAPXSSerializationContext* serialization;
    uint8_t urid;

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer, for async implementation
    uint32_t (*get_new_request_id) (AAPXSInitiatorInstance* instance);

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer
    bool (*send_aapxs_request) (AAPXSInitiatorInstance* instance, AAPXSRequestContext* context);
} AAPXSInitiatorInstance;

// service instance for plugin extension API, and client instance for host extension API
typedef struct AAPXSRecipientInstance {
    // owned by each AAPXS implementation
    void* aapxs_context;
    // owned by hosting implementation
    void* host_context;
    AAPXSSerializationContext* serialization;

    // assigned by: framework reference implementation
    // invoked by: AAPXS developer
    void (*send_aapxs_reply) (AAPXSRecipientInstance* instance, AAPXSRequestContext* context);
} AAPXSRecipientInstance;

struct AAPXSExtensionClientProxy;
struct AAPXSExtensionServiceProxy;

/**
 * The "untyped" AAPXS definition (in the public API surface).
 * Each AAPXS needs to provide an instance of this type so that host framework (reference
 * implementation) can register at its AAPXS map.
 *
 * Instance of this type must be copyable.
 *
 * This type provides a handful of handler functions to deal with AAPXS requests and replies, for
 * both the "plugin extension" and the "host extension".
 * They are implemented by each AAPXS implementation, and invoked by the host framework
 * (reference implementation) that would delegate to each strongly-typed AAPXS function (which is
 * hidden behind `aapxs_context` opaque pointer).
 */
typedef struct AAPXSDefinition {
    /** An opaque pointer to the AAPXS developers' context.
     * It should be assigned and used only by the AAPXS developers of the extension.
     */
    void *aapxs_context;

    /** The extension URI */
    const char *uri;

    /**
     * The data capacity that is used to allocate shared memory for the binary transfer storage (Binder/SysEx8).
     */
    int32_t data_capacity;

    /**
     * Invoked by host (reference implementation) when a plugin extension request has arrived at the service
     * and the service identified which AAPXS handles it.
     *
     * The parameter `definition` provides access to aapxs_context to help AAPXS developers encapsulate the implementation details.
     *
     * @param definition The containing AAPXSDefinition.
     * @param aapxsInstance
     * @param plugin The target plugin
     * @param request The request context that provides access to data, opcode, request ID, etc.
     */
    void (*process_incoming_plugin_aapxs_request) (
            struct AAPXSDefinition* definition,
            AAPXSRecipientInstance* aapxsInstance,
            AndroidAudioPlugin* plugin,
            AAPXSRequestContext* request);
    void (*process_incoming_host_aapxs_request) (
            struct AAPXSDefinition* definition,
            AAPXSRecipientInstance* aapxsInstance,
            AndroidAudioPluginHost* host,
            AAPXSRequestContext* request);
    void (*process_incoming_plugin_aapxs_reply) (
            struct AAPXSDefinition* definition,
            AAPXSInitiatorInstance* aapxsInstance,
            AndroidAudioPlugin* plugin,
            AAPXSRequestContext* request);
    void (*process_incoming_host_aapxs_reply) (
            struct AAPXSDefinition* definition,
            AAPXSInitiatorInstance* aapxsInstance,
            AndroidAudioPluginHost* host,
            AAPXSRequestContext* request);

    struct AAPXSExtensionClientProxy (*get_plugin_extension_proxy) (
            AAPXSDefinition* definition,
            AAPXSInitiatorInstance *aapxsInstance,
            AAPXSSerializationContext *serialization);
    struct AAPXSExtensionServiceProxy (*get_host_extension_proxy) (
            AAPXSDefinition* definition,
            AAPXSInitiatorInstance *aapxsInstance,
            AAPXSSerializationContext *serialization);
} AAPXSDefinition;

typedef struct AAPXSExtensionClientProxy {
    void* aapxs_context;
    void* (*as_plugin_extension) (AAPXSExtensionClientProxy *proxy);
} AAPXSExtensionClientProxy;

typedef struct AAPXSExtensionServiceProxy {
    void* aapxs_context;
    void* (*as_host_extension) (AAPXSExtensionServiceProxy *proxy);
} AAPXSExtensionServiceProxy;

#endif //AAP_CORE_AAPXS_H
