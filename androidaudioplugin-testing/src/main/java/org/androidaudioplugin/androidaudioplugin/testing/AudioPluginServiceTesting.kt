package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import junit.framework.Assert.assertEquals
import org.androidaudioplugin.hosting.AudioPluginClient
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation

class AudioPluginServiceTesting(private val applicationContext: Context) {

    @Deprecated("Use testPluginServiceInfo", replaceWith = ReplaceWith("testPluginServiceInfo()"))
    fun getPluginServiceInfo() = testPluginServiceInformation {}

    fun testPluginServiceInformation(serviceInfoTest: (serviceInfo: PluginServiceInformation) -> Unit = {}) {
        val audioPluginServiceInfo = AudioPluginHostHelper.getLocalAudioPluginService(applicationContext)
        assertEquals ("packageName", applicationContext.packageName, audioPluginServiceInfo.packageName)
        serviceInfoTest(audioPluginServiceInfo)
    }

    fun testSinglePluginInformation(pluginInfoTest: (info: PluginInformation) -> Unit) {
        testPluginServiceInformation { serviceInfo ->
            assertEquals("There are more than one plugins. Not suitable for this test function", 1, serviceInfo.plugins.size)
            pluginInfoTest(serviceInfo.plugins[0])
        }
    }

    fun basicServiceOperationsForAllPlugins() {
        for (pluginInfo in AudioPluginHostHelper.getLocalAudioPluginService(applicationContext).plugins)
            testInstancingAndProcessing(pluginInfo)
    }

    private fun testInstancingAndProcessing(pluginInfo: PluginInformation) {
        val host = AudioPluginClient(applicationContext)

        for (i in 0..3) {
            // we should come up with appropriate locks...
            var passed = false

            host.pluginInstantiatedListeners.add { instance ->
                passed = true
            }
            host.instantiatePluginAsync(pluginInfo) { _, _ ->

                while (!passed)
                    Thread.sleep(1)

                val floatCount = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
                host.instantiatedPlugins.forEach { instance -> instance.prepare(floatCount, host.defaultControlBufferSizeInBytes) }
                host.instantiatedPlugins.forEach { instance -> instance.activate() }
                host.instantiatedPlugins.forEach { instance -> instance.process() }
                host.instantiatedPlugins.forEach { instance -> instance.deactivate() }
                host.instantiatedPlugins.forEach { instance -> instance.destroy() }

                host.instantiatedPlugins.clear()
                host.pluginInstantiatedListeners.clear()
            }
        }
        host.dispose()
    }
}
