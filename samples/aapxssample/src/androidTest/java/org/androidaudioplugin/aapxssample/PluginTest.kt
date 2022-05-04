package org.androidaudioplugin.aapxssample

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.rule.ServiceTestRule
import org.androidaudioplugin.androidaudioplugin.testing.AudioPluginServiceTesting
import org.junit.Rule
import org.junit.Test

class PluginTest {
    private val applicationContext = ApplicationProvider.getApplicationContext<Context>()
    private val testing = AudioPluginServiceTesting(applicationContext)
    @get:Rule
    val serviceRule = ServiceTestRule()

    @Test
    fun getPluginServiceInfo() {
        testing.getPluginServiceInfo()
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
