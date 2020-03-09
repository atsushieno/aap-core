package org.androidaudioplugin

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.MediumTest
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.rule.ServiceTestRule
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith


class AudioPluginHostHelperTest {
    @Test
    fun queryAudioPluginServices() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val services = AudioPluginHostHelper.queryAudioPluginServices(appContext)
        assert(services.all { s -> s.plugins.size > 0 })
        assert(services.any { s -> s.packageName == "org.androidaudioplugin.aappluginsample" && s.label == "AAPLV2SamplePlugin" })
    }
}