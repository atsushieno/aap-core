package org.androidaudioplugin.js

import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

/**
 * Smoke test for the embedded JS runtime. Neither case needs a connected plugin client, so it runs
 * on any device/emulator: it only proves the choc/QuickJS engine + aap-api.js facade are wired up
 * and round-trip through the JNI bridge.
 */
@RunWith(AndroidJUnit4::class)
class AapAutomationRuntimeTest {
    private val context = InstrumentationRegistry.getInstrumentation().context

    @Before
    fun setUp() {
        AapAutomationRuntime.bootstrap(context)
    }

    @Test
    fun pingRoundTrips() {
        val result = AapAutomationRuntime.runScript("JSON.stringify(aap.ping())")
        assertTrue("unexpected result: $result", result.contains("pong"))
    }

    @Test
    fun discoveryReflectsCatalog() {
        AapAutomationRuntime.setPluginCatalog(
            """[{"pluginId":"com.example.test","displayName":"Test","packageName":"com.example"}]"""
        )
        val result = AapAutomationRuntime.runScript("JSON.stringify(aap.discovery.getPlugins())")
        assertTrue("unexpected result: $result", result.contains("com.example.test"))
    }
}
