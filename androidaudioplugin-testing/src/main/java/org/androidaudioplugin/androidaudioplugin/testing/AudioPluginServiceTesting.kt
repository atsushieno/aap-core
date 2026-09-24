package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import junit.framework.Assert.assertEquals
import junit.framework.Assert.assertTrue
import kotlinx.coroutines.runBlocking
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PluginServiceInformation
import org.androidaudioplugin.hosting.AudioPluginClientBase

class AudioPluginServiceTesting(private val applicationContext: Context) {

    @Deprecated("Use testPluginServiceInfo", replaceWith = ReplaceWith("testPluginServiceInfo()"))
    fun getPluginServiceInfo() = testPluginServiceInformation {}

    // A plugin package may have more than one AudioPluginService (in separate processes).
    private val localServices
        get() = AudioPluginServiceHelper.getLocalAudioPluginServices(applicationContext)

    fun testPluginServiceInformation(serviceInfoTest: (serviceInfo: PluginServiceInformation) -> Unit = {}) {
        val services = localServices
        assertTrue("No AudioPluginService was found in this package", services.isNotEmpty())
        for (audioPluginServiceInfo in services) {
            assertEquals ("packageName", applicationContext.packageName, audioPluginServiceInfo.packageName)
            serviceInfoTest(audioPluginServiceInfo)
        }
    }

    fun testSinglePluginInformation(pluginInfoTest: (info: PluginInformation) -> Unit) {
        testPluginServiceInformation()
        val plugins = localServices.flatMap { it.plugins }
        assertEquals("There are more than one plugins. Not suitable for this test function", 1, plugins.size)
        pluginInfoTest(plugins[0])
    }

    fun basicServiceOperationsForAllPlugins() {
        for (pluginInfo in localServices.flatMap { it.plugins })
            testInstancingAndProcessing(pluginInfo)
    }

    // cycle: number of audio instancing and processing cycle. It is multiplied by numParallelInstances.
    @Suppress("UnnecessaryVariable")
    fun testInstancingAndProcessing(pluginInfo: PluginInformation, cycles: Int = 5) {
        // number of parallel instances
        val numParallelInstances = 3

        val host = AudioPluginClientBase(applicationContext)
        val sampleRate = 48000
        val floatCount = 1024
        val controlBufferSize = 0x10000

        runBlocking {
            host.connectToPluginService(pluginInfo)
        }

        for (i in 0 until cycles) {
            val p = numParallelInstances
            val instances = ((0 until p).map { host.instantiateNativePlugin(pluginInfo) })
            assert(instances.map { it.instanceId }.distinct().size == instances.size )
            (0 until p).forEach { instances[it].prepare(floatCount, sampleRate, controlBufferSize) }
            (0 until p).forEach { instances[it].activate() }
            (0 until p).forEach { instances[it].process(floatCount, 0) }
            (0 until p).forEach { instances[it].deactivate() }
            (0 until p).forEach { instances[it].destroy() }
        }

        host.disconnectPluginService(pluginInfo.packageName, pluginInfo.localName)
        host.dispose()
    }
}
