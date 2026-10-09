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

/**
 * Optional failure-delivery callback for asynchronous AAPXS calls.
 *
 * Unlike `aapxs_completion_callback` (which is part of the public C ABI and signals a *successful*
 * reply), this is an internal framework hook used to deliver failures — request timeout or a dead
 * service — to the initiator. `error` is a human-readable description (never null when invoked).
 * It is invoked at most once per request, and mutually exclusively with the success callback.
 */
typedef void (*aapxs_error_callback) (void* context, void* pluginOrHost, const char* error);

typedef struct AAPXSRequestContext {
    aapxs_completion_callback callback;
    void* callback_user_data;
    AAPXSSerializationContext* serialization;
    uint8_t urid;
    const char * uri;
    uint32_t request_id;
    int32_t opcode;
    // Optional; defaults to null for existing brace-initializers that omit it. Set by the async
    // typed AAPXS layer so timeouts / service death can be reported back to the initiator.
    aapxs_error_callback error_callback;
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

    // Optional, framework-owned cancellation service. Control threads only.
    // A registration owns the lifetime needed by unregister_abort_handler, which
    // must remain callable even after this initiator/framework instance is gone.
    // Unregister excludes future delivery and waits for concurrent delivery;
    // unregistering from the handler itself is supported.
    void* lifecycle_context;
    void* (*register_abort_handler)(AAPXSInitiatorInstance*, void* client_context,
                                   void (*handler)(void*, const char* error));
    void (*unregister_abort_handler)(void* registration);
    // Optional C++ helper pointer (aap::xs::TypedAAPXS*), owned by the extension
    // alongside aapxs_context. Consumers borrow it; they must not create a second
    // standard client or release this helper. Null for implementations without it.
    void* typed_client;
    // Optional framework metadata/services. Borrowed plugin ID lives until
    // teardown; request_metadata_refresh schedules control-thread refresh work.
    const char* plugin_id;
    void (*request_metadata_refresh)(AAPXSInitiatorInstance*);
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
    // Optional immutable framework metadata; host_context remains opaque.
    const char* plugin_id;
} AAPXSRecipientInstance;

struct AAPXSExtensionClientProxy;
struct AAPXSExtensionServiceProxy;

// In-process service dispatch policy; these flags are never serialized.
// Unannotated plugin requests retain exclusive, potentially-mutating dispatch.
enum AAPXSRequestFlags : uint32_t {
    AAPXS_REQUEST_READ_ONLY = 1,
    AAPXS_REQUEST_CONCURRENT = 2, // handler only reads extension-owned immutable/atomic state
    AAPXS_REQUEST_COALESCE = 4,  // payload-free host notification, opcode -1..-32
    AAPXS_REQUEST_STATE_CHANGED = 8 // host notification invalidates extension snapshots
};

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

    /**
     * Determines whether a particular command (request opcode) is real-time safe and may therefore
     * be delivered over the RT-safe AAPXS SysEx8 channel while the plugin is at ACTIVE state.
     *
     * When this returns false, the host framework (reference implementation) routes the request over
     * the synchronous Binder IPC path instead.
     *
     * This function pointer may be nullptr, in which case every command of this extension is treated
     * as RT-unsafe and always goes through Binder. nullptr is therefore the backward-compatible
     * default ("Binder only").
     *
     * IMPORTANT: this may be invoked on the audio thread, so the implementation itself must be
     * RT-safe (no allocation, no locking) - typically just a `switch` over `opcode`.
     *
     * @param definition The containing AAPXSDefinition.
     * @param isHostExtension true if the command targets a host extension (service->host),
     *                        false for a plugin extension (client->plugin).
     * @param opcode The request opcode.
     */
    bool (*is_command_rt_safe) (
            struct AAPXSDefinition* definition,
            bool isHostExtension,
            int32_t opcode);

    /** Native host-extension fallback. The framework's built-in host extensions
     * and the host's own implementation take precedence. This does not create
     * an outgoing-request proxy. Borrowed receiver pointers live until teardown.
     */
    struct AAPXSExtensionHostReceiver (*get_host_extension_receiver) (
            struct AAPXSDefinition* definition,
            AAPXSRecipientInstance *aapxsInstance,
            AndroidAudioPluginHost *host);

    /**
     * Releases what the AAPXS stored in the `aapxs_context` of its per-plugin-instance
     * AAPXSInitiatorInstance / AAPXSRecipientInstance. Invoked once for each non-null context,
     * after the plugin instance is destroyed. May be nullptr if the AAPXS does not use them.
     */
    void (*release_instance_context) (
            struct AAPXSDefinition* definition,
            void* aapxsContext);

    // Extension-owned policy. Must not allocate or lock: notification producers
    // can query it from processing. nullptr uses the conservative default.
    uint32_t (*get_request_flags)(AAPXSDefinition*, bool isHostExtension, int32_t opcode);

    // Called under control exclusion after setup/prepare, state changes and
    // potentially-mutating requests. Each extension owns its recipient context
    // and any snapshots; the runtime does not know their contents or opcodes.
    void (*on_plugin_state_changed)(AAPXSDefinition*, AAPXSRecipientInstance*, AndroidAudioPlugin*);
    void (*release_plugin_instance_context)(AAPXSDefinition*, void*);
    // Invoked for the actual outgoing request, after any notification handoff.
    // Extension-specific local effects belong here, not in the dispatcher.
    void (*on_outgoing_host_request)(AAPXSDefinition*, AAPXSInitiatorInstance*, AAPXSRequestContext*);

    /** Control/setup lifecycle, never processing. is_host_extension describes
     * the request target: false for client->plugin, true for plugin->host.
     * The dispatcher initializes stable instance structs after mapping buffers
     * and installing framework services, before publishing proxies or processing.
     * Implementations own aapxs_context; definitions must not own instance state.
     * Returning false rejects setup; contexts created so far are released.
     */
    bool (*initialize_initiator_instance)(AAPXSDefinition*, AAPXSInitiatorInstance*, bool is_host_extension);
    bool (*initialize_recipient_instance)(AAPXSDefinition*, AAPXSRecipientInstance*, bool is_host_extension);
    /** Called once for each non-null context, after plugin release and before
     * instance structs/buffers are destroyed. Do not access host_context or send
     * requests here. The dispatcher clears aapxs_context after return.
     * These callbacks supersede the legacy release callbacks for that role.
     */
    void (*release_initiator_instance)(AAPXSDefinition*, AAPXSInitiatorInstance*, bool is_host_extension);
    void (*release_recipient_instance)(AAPXSDefinition*, AAPXSRecipientInstance*, bool is_host_extension);
} AAPXSDefinition;

typedef struct AAPXSExtensionClientProxy {
    void* aapxs_context;
    void* (*as_plugin_extension) (AAPXSExtensionClientProxy *proxy);
} AAPXSExtensionClientProxy;

typedef struct AAPXSExtensionServiceProxy {
    void* aapxs_context;
    void* (*as_host_extension) (AAPXSExtensionServiceProxy *proxy);
} AAPXSExtensionServiceProxy;

typedef struct AAPXSExtensionHostReceiver {
    void* aapxs_context;
    void* (*as_host_extension) (AAPXSExtensionHostReceiver *receiver);
} AAPXSExtensionHostReceiver;

#endif //AAP_CORE_AAPXS_H
