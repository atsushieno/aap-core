package org.androidaudioplugin.midideviceservice

import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.androidaudioplugin.PluginInformation

import org.junit.Test
import org.junit.runner.RunWith

import org.junit.Assert.*

/**
 * Instrumented test, which will execute on an Android device.
 *
 * See [testing documentation](http://d.android.com/tools/testing).
 */
@RunWith(AndroidJUnit4::class)
class ExampleInstrumentedTest {
    @Test
    fun useAppContext() {
        // Context of the app under test.
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        assertEquals("org.androidaudioplugin.aap_midi_device_service", appContext.packageName)

        applicationContextForModel = appContext
        val model = ApplicationModel(appContext.packageName, appContext)

        val plugins = org.androidaudioplugin.midideviceservice.model.pluginServices.flatMap { s -> s.plugins }.toList()
            .filter { p -> p.category?.contains("Instrument") ?: false || p.category?.contains("Synth") ?: false }

        for (plugin in plugins) {
            model.specifiedInstrument = plugin
            model.initializeMidi()
            model.playNote()
            model.terminateMidi()
        }
    }
}