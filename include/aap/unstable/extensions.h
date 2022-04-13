#ifndef AAP_EXTENSIONS_H_INCLUDED
#define AAP_EXTENSIONS_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif
#include <stdint.h>
#include "aap-core.h"

/**
 * The public extension API surface that represents a client, for host developers.
 */
typedef struct {
    AndroidAudioPluginExtension* get_extension_proxy(const char *uri);
} AndroidAudioPluginClient;

typedef void (*aap_plugin_extension_service_client_extension_message_t) (void* context, int32_t opcode);

/**
 * The public extension API surface that represents a service extension instance, for plugin extension service implementors.
 *
 */
typedef struct {
    void *context;
    const char *uri;
    AndroidAudioPluginExtension *data;
} AndroidAudioPluginServiceExtension;

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
        AndroidAudioPluginExtensionServiceClient *client,
        AndroidAudioPluginServiceExtension* serviceExtension,
        int32_t opcode);

typedef void* (*aap_service_extension_as_proxy) (AndroidAudioPluginExtensionServiceClient *client,
                                                 AndroidAudioPluginServiceExtension* serviceExtension);

typedef struct {
    const char *uri;
    aap_service_extension_on_invoked_t on_invoked;
    aap_service_extension_as_proxy as_proxy;
} aap_service_extension_t;

typedef aap_service_extension_t* (*aap_service_extension_factory_create_extension_instance_t) (
        void* context);

typedef struct {
    const char *uri;
    aap_service_extension_factory_create_extension_instance_t create_instance;
} AndroidAudioPluginServiceExtensionFactory;

void aap_register_service_extension_factory(AndroidAudioPluginServiceExtensionFactory *factory);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_EXTENSIONS_H_INCLUDED */




