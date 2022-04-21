
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

} // namespace aap
