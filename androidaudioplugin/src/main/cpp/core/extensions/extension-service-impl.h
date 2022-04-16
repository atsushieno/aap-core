
#include "aap/core/host/extension-service.h"


namespace aap {

//-----------------------------------

class AndroidAudioPluginClientExtensionImpl {
protected:
    AndroidAudioPluginExtensionServiceClient *client;
    AndroidAudioPluginExtension *extensionInstance;

public:
    AndroidAudioPluginClientExtensionImpl(AndroidAudioPluginExtensionServiceClient *extensionClient,
                                          AndroidAudioPluginExtension *extensionInstance)
            : client(extensionClient), extensionInstance(extensionInstance) {
    }

    /** Optionally override this for additional initialization and resource acquisition */
    virtual void initialize() {}

    /** Optionally override this for additional termination and resource releases */
    virtual void terminate() {}

    void clientInvokePluginExtension(int32_t opcode) {
        // FIXME: implement
    }

    virtual void* asProxy() = 0;
};

class AndroidAudioPluginServiceExtensionImpl {
protected:
    AndroidAudioPluginExtension *extensionInstance{nullptr};

public:
    explicit AndroidAudioPluginServiceExtensionImpl(AndroidAudioPluginExtension *extensionInstance)
            : extensionInstance(extensionInstance) {
    }

    /** Optionally override this for additional initialization and resource acquisition */
    virtual void initialize() {}

    /** Optionally override this for additional termination and resource releases */
    virtual void terminate() {}

    virtual void onInvoked(aap::LocalPluginInstance *instance,
                           int32_t opcode) = 0;
};

} // namespace aap
