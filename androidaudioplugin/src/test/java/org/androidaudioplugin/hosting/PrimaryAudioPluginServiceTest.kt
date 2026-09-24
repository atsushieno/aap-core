package org.androidaudioplugin.hosting

import org.androidaudioplugin.AudioPluginService
import org.androidaudioplugin.PluginServiceInformation
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class PrimaryAudioPluginServiceTest {
    private val packageName = "org.example.plugins"

    private fun service(className: String) = PluginServiceInformation("label", packageName, className)

    @Test
    fun stockClassIsPrimary() {
        val services = listOf(service("org.example.plugins.SecondAudioPluginService"),
            service(AudioPluginService::class.java.name))
        assertEquals(AudioPluginService::class.java.name,
            AudioPluginHostHelper.selectPrimaryAudioPluginService(services)?.className)
    }

    @Test
    fun firstServiceWithoutStockClass() {
        val services = listOf(service("org.example.plugins.FirstAudioPluginService"),
            service("org.example.plugins.SecondAudioPluginService"))
        assertEquals("org.example.plugins.FirstAudioPluginService",
            AudioPluginHostHelper.selectPrimaryAudioPluginService(services)?.className)
    }

    @Test
    fun noService() {
        assertNull(AudioPluginHostHelper.selectPrimaryAudioPluginService(listOf()))
    }
}
