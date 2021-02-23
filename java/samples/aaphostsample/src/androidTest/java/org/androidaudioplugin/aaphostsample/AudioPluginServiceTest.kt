package org.androidaudioplugin.aaphostsample

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import org.androidaudioplugin.AudioPluginHost
import org.androidaudioplugin.AudioPluginHostHelper
import org.junit.Rule
import org.junit.Test
import java.lang.Thread.sleep
import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class AudioPluginServiceTest {
    val applicationContext = ApplicationProvider.getApplicationContext<Context>()

    @Test
    fun basicServiceOperations() {
        val pluginInfo = AudioPluginHostHelper.queryAudioPlugins(applicationContext).first {
                s -> s.pluginId == "urn:org.androidaudioplugin/samples/aapbarebonepluginsample/FlatFilter" }
        val host = AudioPluginHost(applicationContext)

        // we should come up with appropriate locks...
        var passed = false
        host.pluginInstantiatedListeners.add { instance ->
            val floatCount = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
            instance.prepare(host.sampleRate, floatCount)

            instance.activate()
            instance.process()
            instance.deactivate()
            instance.destroy()
            passed = true
        }
        host.instantiatePlugin(pluginInfo)

        while (!passed)
            sleep(1)
    }
}
