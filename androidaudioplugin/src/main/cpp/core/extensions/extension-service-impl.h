
#ifndef AAP_EXTENSIONS_SERVICE_IMPL_H
#define AAP_EXTENSIONS_SERVICE_IMPL_H

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
        clientInstance->extension_message(clientInstance, opcode);
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
    std::unique_ptr<const char> uri;
    std::unique_ptr<AAPXSFeature> pub;

    static AAPXSProxyContext internalAsProxy(AAPXSFeature *feature, AAPXSClientInstance* extension) {
        auto thisObj = (PluginExtensionFeatureImpl*) feature->context;
        assert(thisObj);
        auto impl = thisObj->getClient();
        assert(impl);
        return AAPXSProxyContext {extension, impl->asProxy(extension) };
    }

    static void internalOnInvoked(AAPXSFeature* feature, void *instance, AAPXSServiceInstance* extension, int32_t opcode) {
        auto thisObj = (PluginExtensionFeatureImpl*) feature->context;
        thisObj->getService()->onInvoked(instance, extension, opcode);
    }

public:
    PluginExtensionFeatureImpl(const char *extensionUri, int32_t sharedMemorySize)
            : uri(strdup(extensionUri)),
            pub(std::make_unique<AAPXSFeature>()) {
        pub->uri = uri.get();
        pub->shared_memory_size = sharedMemorySize;
        pub->context = this;
        pub->as_proxy = internalAsProxy;
        pub->on_invoked = internalOnInvoked;
    }

    virtual ~PluginExtensionFeatureImpl() {}

    AAPXSFeature* asPublicApi() { return pub.get(); }

    virtual PluginClientExtensionImplBase* getClient() = 0;
    virtual PluginServiceExtensionImplBase* getService() = 0;
};


} // namespace aap


#endif // AAP_EXTENSIONS_SERVICE_IMPL_H
