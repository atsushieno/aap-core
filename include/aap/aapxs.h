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
#include "android-audio-plugin.h"

/**
 * The public extension API surface that represents a service extension instance, for plugin extension service implementors.
 * In libandroidaudioplugin implementation, its `context` is `aap::PluginService, which has handful of members:
 *
 * - serviceHandle, which is AudioPluginInterfaceImpl on Android.
 */
typedef struct {
    /** AAPXS implementation context that is used and should be assigned only by the extension developer. */
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
    /** extension-specific local context. Extension developer uses it. */
    void *local_data;
} AAPXSServiceInstance;

// ---------------------------------------------------

/**
 * An instantiated AAPXS helper for a client.
 * It is implemented by AAP framework.
 * In libandroidaudioplugin reference implementation, it is `AAPXSClientInstanceManager` that manages
 * the instances of this struct.
 */
typedef struct AAPXSClientInstance {
    /** Custom context that AAP framework may assign.
     * In libandroidaudioplugin it is RemotePluginInstance. It may be different on other implementations.
     */
    void *host_context;

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

    /**
     * Do send the extension() request (either send via Binder, or add AAPXS SysEx8 to the MIDI2 channel buffer)
     *
     * Can be used for either synchronous or asynchronous calls.
     */
     // FIXME: rename this to `send_extension_request`
    void (*extension_message) (struct AAPXSClientInstance* aapxsClientInstance, int32_t messageSize, int32_t opcode);

    /** Handle the reply */
    void (*handle_extension_reply) (struct AAPXSClientInstance* aapxsClientInstance, int32_t messageSize, int32_t opcode, int32_t requestId);
} AAPXSClientInstance;

// ---------------------------------------------------

typedef AndroidAudioPlugin* (*aapxs_host_context_get_plugin_t) (void* frameworkContext);

typedef struct AAPXSProxyContext {
    /** It is assigned by the extension hosting implementation.
     *  In libandroidaudioplugin, AAPXSClientInstance holds RemoteClientInstance as its context.
     *  Along with the instance ID, it is used to retrieve AndroidAudioPlugin. */
    AAPXSClientInstance *host_context;
    /** The actual extension proxy of the same type as non-AAPXS extension. */
    void *aapxs_context;
    void *extension;
} AAPXSProxyContext;

/**
 * The entrypoint for AAP extension service feature.
 * Every extension developer defines one for each AAPXS (i.e. instantiated for each extension).
 * Multiple clients may share the same instance of this struct. They pass client instances.
 */
typedef struct AAPXSFeature {

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
    void (*on_invoked) (struct AAPXSFeature* feature,
                        AndroidAudioPlugin* plugin,
                        AAPXSServiceInstance* extension,
                        int32_t opcode);
    /**
     * Implemented by the extension developer.
     * Called by AAP framework (client part) to return the extension proxy to the extension.
     * Client application has access to the extension service only through the extension API via the proxy.
     */
    AAPXSProxyContext (*as_proxy) (struct AAPXSFeature* feature, AAPXSClientInstance* extension);

    /** True if get_extension() must return non-null implementation. */
    bool is_implementation_mandatory;
} AAPXSFeature;

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* AAP_EXTENSIONS_H_INCLUDED */
