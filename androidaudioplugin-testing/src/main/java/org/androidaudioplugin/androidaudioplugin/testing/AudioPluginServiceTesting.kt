package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import junit.framework.Assert.assertEquals
import org.androidaudioplugin.AudioPluginHost
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation

class AudioPluginServiceTesting(private val applicationContext: Context) {

    fun getPluginServiceInfo() {
        val audioPluginServiceInfo = AudioPluginHostHelper.getLocalAudioPluginService(applicationContext)
        assertEquals ("packageName", applicationContext.packageName, audioPluginServiceInfo.packageName)
    }

    fun basicServiceOperationsForAllPlugins() {
        for (pluginInfo in AudioPluginHostHelper.getLocalAudioPluginService(applicationContext).plugins)
            testInstancingAndProcessing(pluginInfo)
    }

    private fun testInstancingAndProcessing(pluginInfo: PluginInformation) {
        val host = AudioPluginHost(applicationContext)

        for (i in 0..3) {
            // we should come up with appropriate locks...
            var passed = false

            host.pluginInstantiatedListeners.add { instance ->
                passed = true
            }
            host.instantiatePlugin(pluginInfo)

            while (!passed)
                Thread.sleep(1)

            val floatCount = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
            host.instantiatedPlugins.forEach { instance -> instance.prepare(floatCount) }
            host.instantiatedPlugins.forEach { instance -> instance.activate() }
            host.instantiatedPlugins.forEach { instance -> instance.process() }
            host.instantiatedPlugins.forEach { instance -> instance.deactivate() }
            host.instantiatedPlugins.forEach { instance -> instance.destroy() }

            host.instantiatedPlugins.clear()
            host.pluginInstantiatedListeners.clear()
        }
        host.dispose()
    }
}
