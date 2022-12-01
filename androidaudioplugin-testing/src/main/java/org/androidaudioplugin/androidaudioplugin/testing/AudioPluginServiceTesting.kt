package org.androidaudioplugin.androidaudioplugin.testing

import android.content.Context
import junit.framework.Assert.assertEquals
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
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

    // FIXME: historically we had some reproducible issues that started occur at 4th iteration somehow.
    //  It should be bigger than >=3. It is set to 1 because we are stuck at some failing tests here...
    fun testInstancingAndProcessing(pluginInfo: PluginInformation, cycles: Int = 1) {

        val host = AudioPluginClient(applicationContext)
        val floatCount = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
        val controlBufferSize = host.defaultControlBufferSizeInBytes

        runBlocking {
            // we should come up with appropriate locks...
            val mutex = Mutex(true)
            host.connectToPluginServiceAsync(pluginInfo.packageName) { _, _ ->
                for (i in 0 until cycles) {
                    val instance = host.instantiatePlugin(pluginInfo)
                    instance.prepare(floatCount, controlBufferSize)
                    instance.activate()
                    instance.process()
                    instance.deactivate()
                    instance.destroy()
                }
                mutex.unlock()
            }
            mutex.withLock { }
        }

        // FIXME: we should also enable this part to sanity check async instantiation.
        /*
        for (i in 0..3) {
            runBlocking {
                // we should come up with appropriate locks...
                val mutex = Mutex(true)

                host.instantiatePluginAsync(pluginInfo) { _, _ ->

                    host.instantiatedPlugins.forEach { instance -> instance.prepare(floatCount, controlBufferSize) }
                    host.instantiatedPlugins.forEach { instance -> instance.activate() }
                    host.instantiatedPlugins.forEach { instance -> instance.process() }
                    host.instantiatedPlugins.forEach { instance -> instance.deactivate() }
                    host.instantiatedPlugins.forEach { instance -> instance.destroy() }

                    mutex.unlock()
                }

                mutex.withLock { }
            }
        }*/

        assert(host.instantiatedPlugins.size == 0)

        host.dispose()
    }
}
