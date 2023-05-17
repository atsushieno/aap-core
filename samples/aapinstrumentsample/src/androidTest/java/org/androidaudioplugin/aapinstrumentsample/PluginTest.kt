package org.androidaudioplugin.aapinstrumentsample

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.rule.ServiceTestRule
import org.androidaudioplugin.AudioPluginServiceHelper
import org.androidaudioplugin.androidaudioplugin.testing.AudioPluginServiceTesting
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.junit.Assert
import org.junit.Rule
import org.junit.Test

class PluginTest {
    private val applicationContext = ApplicationProvider.getApplicationContext<Context>()
    private val testing = AudioPluginServiceTesting(applicationContext)
    @get:Rule
    val serviceRule = ServiceTestRule()

    @Test
    fun testPluginInfo() {
        testing.testSinglePluginInformation {
            Assert.assertEquals("urn:org.androidaudioplugin/samples/aapinstrumentsample/InstrumentSample", it.pluginId)
            Assert.assertEquals("AAP Developers", it.developer)
            Assert.assertEquals(5, it.parameters.size)
            Assert.assertEquals(0, it.ports.size)
        }
    }

    @Test
    fun basicServiceOperationsForAllPlugins() {
        testing.basicServiceOperationsForAllPlugins()
    }

    @Test
    fun repeatDirectServiceOperations() {
        val pluginInfo = AudioPluginServiceHelper.getLocalAudioPluginService(applicationContext).plugins.first()

        for (i in 0 until 5)
            testing.testInstancingAndProcessing(pluginInfo)
    }
}
