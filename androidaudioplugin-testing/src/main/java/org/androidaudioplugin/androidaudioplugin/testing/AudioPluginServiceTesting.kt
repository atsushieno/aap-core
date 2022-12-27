package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import junit.framework.Assert.assertEquals
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.hosting.AudioPluginClientBase
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginInstance

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

    // cycle: number of audio instancing and processing cycle. It is multiplied by numParallelInstances.
    @Suppress("UnnecessaryVariable")
    fun testInstancingAndProcessing(pluginInfo: PluginInformation, cycles: Int = 5) {
        // number of parallel instances
        val numParallelInstances = 3

        val host = AudioPluginClientBase(applicationContext)
        val floatCount = 1024
        val controlBufferSize = 0x10000

        runBlocking {
            host.connectToPluginService(pluginInfo.packageName)
        }

        for (i in 0 until cycles) {
            val p = numParallelInstances
            val instances = ((0 until p).map { host.instantiatePlugin(pluginInfo) })
            assert(instances.map { it.instanceId }.distinct().size == instances.size )
            (0 until p).forEach { instances[it].prepare(floatCount, controlBufferSize) }
            (0 until p).forEach { instances[it].activate() }
            (0 until p).forEach { instances[it].process() }
            (0 until p).forEach { instances[it].deactivate() }
            (0 until p).forEach { instances[it].destroy() }
        }

        assert(host.instantiatedPlugins.size == 0)

        host.disconnectPluginService(pluginInfo.packageName)
        host.dispose()
    }
}
