package org.androidaudioplugin.aaphostsample

import androidx.test.platform.app.InstrumentationRegistry
import org.androidaudioplugin.AudioPluginHostHelper
import org.junit.Test


class AudioPluginHostHelperTest {
    @Test
    fun queryAudioPluginServices() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        val services = AudioPluginHostHelper.queryAudioPluginServices(appContext)
        assert(services.all { s -> s.plugins.size > 0 })
        assert(services.any { s -> s.packageName == "org.androidaudioplugin.aapbarebonepluginsample" && s.label == "AAPBareBoneSamplePlugin" })
    }
}