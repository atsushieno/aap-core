package org.androidaudioplugin.hosting

/*
  This class wraps native aap::PluginClient and provides the corresponding features to the native API, to some extent.
 */
class NativePluginClient(private val sampleRate: Int, val serviceConnector: AudioPluginServiceConnector) {

    // aap::PluginClient*
    val native: Long = newInstance(serviceConnector.instanceId)

    fun createInstanceFromExistingConnection(pluginId: String) : NativeRemotePluginInstance {
        return NativeRemotePluginInstance(pluginId, sampleRate, this)
    }

    companion object {
        @JvmStatic
        private external fun newInstance(connectorInstanceId: Int) : Long
    }
}