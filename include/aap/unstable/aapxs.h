#ifndef AAP_EXTENSIONS_H_INCLUDED
#define AAP_EXTENSIONS_H_INCLUDED

/*
 * AAPXS - AAP Extension Service public API which is open to (available for) extension developers.
 * AAP internals wrap those types in its own.
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "aap-core.h"

struct AAPXSClient;

/**
 * The public extension API surface that represents a service extension instance, for plugin extension service implementors.
 * In libandroidaudioplugin implementation, its `context` is `aap::PluginService, which has handful of members:
 *
 * - serviceHandle, which is AudioPluginInterfaceImpl on Android.
 */
typedef struct {
    void *context;
    /** The extension URI */
    const char *uri;
    /** The plugin instance ID within the service.
     * (Note that two or more plugins can be instantiated from the same AudioPluginService.)
     */
    int32_t plugin_instance_id;
    /** The shared memory pointer, if is provided. */
    void *data;
    /** The size of `data`, if it is provided a non-null pointer. */
    int32_t data_size;
} AAPXSServiceInstance;

/**
 * The public extension API surface that represents a service extension instance, for plugin extension client implementors.
 * In libandroidaudioplugin implementation, its `context` is `aap::PluginClient, which has handful of members:
 *
 * - clientHandle; on Android it is AIBinder* as the result of bindService().
 * - provides access to `extension()` AIDL-based request that extension developers can invoke.
 */
typedef struct AAPXSClientInstance {
    /** Custom context that AAP framework may assign. */
    void *context;

    /** The client that this plugin extension service instance belongs to. */
    struct AAPXSClient *client;

    /** The extension URI */
    const char *uri;

    /** The plugin instance ID within the client.
     * (Note that two or more plugins can be instantiated from the same AudioPluginService.)
     */
    int32_t plugin_instance_id;

    /** The shared memory pointer, if it needs to provide. */
    void *data;

    /** The size of `data`, if it provides non-null pointer. */
    int32_t data_size;

} AAPXSClientInstance;

// ---------------------------------------------------

typedef void (*aap_plugin_extension_service_client_extension_message_t) (
        void* context,
        AAPXSClientInstance* aapxsClientInstance,
        int32_t opcode);

/**
 * The public extension API surface that works as a facade of the host client, for plugin extension service implementors.
 * In libandroidaudioplugin implementation, its `context` is `aap::RemotePluginInstance, which has handful of members:
 *
 * - `sendExtensionMessage()`, which calls `extension()` AIDL-based request.
 * - instanceId that is needed to pass to `extension()`
 */
typedef struct AAPXSClient {
    void *context;
    aap_plugin_extension_service_client_extension_message_t extension_message;
} AAPXSClient;

// ---------------------------------------------------

typedef void (*aapxs_feature_on_invoked_t) (
        /** opaque pointer to the plugin service, provided by AAP framework. */
        void *service,
        AAPXSServiceInstance* extension,
        int32_t opcode);

typedef void* (*aapxs_feature_as_proxy_t) (
        AAPXSClientInstance* extension);

/**
 * The entrypoint for AAP extension service feature.
 * Every extension developer defines one for each AAPXS (i.e. instantiated for each extension).
 * Multiple clients may share the same instance of this struct. They pass client instances.
 */
typedef struct {
    const char *uri;
    /**
     * Implemented by the extension developer.
     * Called by AAP framework (service part) to invoke the actual plugin extension.
     * Plugin (service) has direct (in-memory) access to the extension, but arguments are the ones
     * deserialized by the implementation of the extension, not the ones in the host memory space.
     */
    aapxs_feature_on_invoked_t on_invoked;
    /**
     * Implemented by the extension developer.
     * Called by AAP framework (client part) to return the extension proxy to the extension.
     * Client application has access to the extension service only through the extension API via the proxy.
     */
    aapxs_feature_as_proxy_t as_proxy;
} AAPXSFeature;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_EXTENSIONS_H_INCLUDED */
