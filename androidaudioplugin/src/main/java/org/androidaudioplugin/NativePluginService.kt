package org.androidaudioplugin

/*
  This class wraps native aap::PluginService and provides the corresponding features to the native API, to some extent.
 */
class NativePluginService(pluginId: String) {
    internal val native: Long

    fun getInstance(instanceId: Int) = NativeLocalPluginInstance(this, instanceId)

    init {
        native = AudioPluginServiceHelper.getServiceInstance(pluginId)
        assert(native != 0L)
    }
}
