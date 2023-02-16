package org.androidaudioplugin.aapbarebonepluginsample

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ServiceTestRule
import org.androidaudioplugin.hosting.AudioPluginHostHelper
import org.androidaudioplugin.androidaudioplugin.testing.AudioPluginServiceTesting
import org.junit.Assert
import org.junit.Rule
import org.junit.Test

class PluginTest {
    private val applicationContext = ApplicationProvider.getApplicationContext<Context>()
    private val testing = AudioPluginServiceTesting(applicationContext)
    @get:Rule
    val serviceRule = ServiceTestRule()

    @Test
    fun queryAudioPluginServices() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val services = AudioPluginHostHelper.queryAudioPluginServices(appContext)
        assert(services.all { s -> s.plugins.size > 0 })
        assert(services.any { s -> s.packageName == "org.androidaudioplugin.aapbarebonepluginsample" && s.label == "AAPBareBoneSamplePlugin" })
    }

    @Test
    fun testPluginInfo() {
        testing.testSinglePluginInformation {
            Assert.assertEquals("urn:org.androidaudioplugin/samples/aapbarebonepluginsample/TestFilter", it.pluginId)
            Assert.assertEquals("AAP Developers", it.developer)
            Assert.assertEquals(8, it.parameters.size)
            Assert.assertEquals(0, it.ports.size)
        }
    }

    @Test
    fun basicServiceOperationsForAllPlugins() {
        testing.basicServiceOperationsForAllPlugins()
    }

    @Test
    fun repeatDirectServieOperations() {
        for (i in 0 until 5)
            basicServiceOperationsForAllPlugins()
    }
}
