package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import junit.framework.Assert.assertEquals
import org.androidaudioplugin.AudioPluginHost
import org.androidaudioplugin.AudioPluginHostHelper
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

        // we should come up with appropriate locks...
        var passed = false
        host.pluginInstantiatedListeners.add { instance ->
            val floatCount = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
            instance.prepare(floatCount)

            instance.activate()
            instance.process()
            instance.deactivate()
            instance.destroy()
            passed = true
        }
        host.instantiatePlugin(pluginInfo)

        while (!passed)
            Thread.sleep(1)
    }
}
