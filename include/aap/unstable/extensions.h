#ifndef AAP_EXTENSIONS_H_INCLUDED
#define AAP_EXTENSIONS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "aap-core.h"

/**
 * The public extension API surface that represents a service extension instance, for plugin extension service implementors.
 *
 */
typedef struct {
    void *context;
    const char *uri;
    AndroidAudioPluginExtension *data;
} AndroidAudioPluginServiceExtension;

typedef void (*aap_plugin_extension_service_client_extension_message_t) (void* context, int32_t opcode);

/**
 * The public extension API surface that works as a facade of the host client, for plugin extension service implementors.
 * In libandroidaudioplugin implementation, its `context` is `aap::PluginClient, which has handful of members:
 *
 * - serviceHandle, which is AIBinder* on Android.
 * - provides access to `extension()` AIDL-based request.
 * - instanceId that is needed to pass to `extension()`
 */
typedef struct {
    void *context;
    aap_plugin_extension_service_client_extension_message_t extension_message;
} AndroidAudioPluginExtensionServiceClient;

typedef void (*aap_service_extension_on_invoked_t) (
        /** opaque pointer to the plugin service, provided by AAP framework */
        void *service,
        AndroidAudioPluginServiceExtension* serviceExtension,
        int32_t opcode);

typedef void* (*aap_service_extension_as_proxy) (AndroidAudioPluginExtensionServiceClient *client,
                                                 AndroidAudioPluginServiceExtension* serviceExtension);

/**
 * The facade for AAP extension service feature.
 * Instantiated for each extension.
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
    aap_service_extension_on_invoked_t on_invoked;
    /**
     * Implemented by the extension developer.
     * Called by AAP framework (client part) to return the extension proxy to the extension.
     * Client application has access to the extension service only through the extension API via the proxy.
     */
    aap_service_extension_as_proxy as_proxy;
} AndroidAudioPluginExtensionFeature;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_EXTENSIONS_H_INCLUDED */




