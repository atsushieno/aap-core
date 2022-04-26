
#include "aap/core/host/extension-service.h"


namespace aap {

//-----------------------------------

/**
 * An abstract class to help implementing AAPXS client part i.e. `asProxy()` in C++.
 */
class PluginClientExtensionImplBase {

public:
    virtual ~PluginClientExtensionImplBase() {}

    /** Optionally override this for additional initialization and resource acquisition */
    virtual void initialize() {}

    /** Optionally override this for additional termination and resource releases */
    virtual void terminate() {}

    void clientInvokePluginExtension(AAPXSClientInstance* clientInstance, int32_t opcode) {
        clientInstance->client->extension_message(this, clientInstance, opcode);
    }

    virtual void* asProxy(AAPXSClientInstance *clientInstance) = 0;
};

/**
 * An abstract class to help implementing AAPXS service part i.e. `onInvoked()` in C++.
 */
class PluginServiceExtensionImplBase {
    AAPXSServiceInstance pub;

public:
    PluginServiceExtensionImplBase(const char *uri) {
        pub.context = this;
        pub.uri = uri;
    }

    virtual ~PluginServiceExtensionImplBase() {}

    /** Optionally override this for additional initialization and resource acquisition */
    virtual void initialize() {}

    /** Optionally override this for additional termination and resource releases */
    virtual void terminate() {}

    // `instance` is actually aap::LocalPluginInstance, but we don't expose the host type to extension developers.
    virtual void onInvoked(void *contextInstance, AAPXSServiceInstance *extensionInstance,
                           int32_t opcode) = 0;

    AAPXSServiceInstance asTransient() {
        return pub;
    }
};

/**
 * Used by internal extension developers (that is, AAP framework developers)
 */
class PluginExtensionFeatureImpl {
    std::string uri;
    std::unique_ptr<PluginClientExtensionImplBase> client;
    std::unique_ptr<PluginServiceExtensionImplBase> service;
    AAPXSFeature pub;

    static void* internalAsProxy(/*AAPXSFeature feature,*/ AAPXSClientInstance* extension) {
        a_log(AAP_LOG_LEVEL_INFO, "AAPXS", "INTERNAL_AS_PROXY 1");
        auto thisObj = (PluginExtensionFeatureImpl*) nullptr;//feature.context;
        assert(thisObj);
        a_log(AAP_LOG_LEVEL_INFO, "AAPXS", "INTERNAL_AS_PROXY 2");
        auto impl = thisObj->client.get();
        assert(thisObj);
        a_log(AAP_LOG_LEVEL_INFO, "AAPXS", "INTERNAL_AS_PROXY 3");
        return impl->asProxy(extension);
    }

    static void internalOnInvoked(/*AAPXSFeature feature,*/ void *service, AAPXSServiceInstance* extension, int32_t opcode) {
        auto thisObj = (PluginExtensionFeatureImpl*) nullptr;//feature.context;
        thisObj->service->onInvoked(service, extension, opcode);
    }

public:
    PluginExtensionFeatureImpl(const char *extensionUri,
                               std::unique_ptr<PluginClientExtensionImplBase> clientImpl,
                               std::unique_ptr<PluginServiceExtensionImplBase> serviceImpl
    )
            : uri(extensionUri), client(std::move(clientImpl)), service(std::move(serviceImpl)) {
        pub.uri = uri.c_str();
        //pub.context = this;
        pub.as_proxy = internalAsProxy;
        pub.on_invoked = internalOnInvoked;
    }

    AAPXSFeature asPublicApi() { return pub; }
};

} // namespace aap
