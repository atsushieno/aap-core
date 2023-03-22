package org.androidaudioplugin.hosting

/*
  This class wraps native aap::PluginClient and provides the corresponding features to the native API, to some extent.
 */
class NativePluginClient(val native: Long) {

    // depending on how the native instance is created, it may or may not be invoked from Kotlin code.
    fun dispose() = destroyInstance(native)

    fun createInstanceFromExistingConnection(sampleRate: Int, pluginId: String) : NativeRemotePluginInstance {
        return NativeRemotePluginInstance.create(pluginId, sampleRate, native)
    }

    companion object {
        fun createFromConnection(serviceConnectionId: Int) = NativePluginClient(newInstance(serviceConnectionId))

        @JvmStatic
        private external fun newInstance(serviceConnectionId: Int) : Long

        @JvmStatic
        private external fun destroyInstance(native: Long)
    }
}