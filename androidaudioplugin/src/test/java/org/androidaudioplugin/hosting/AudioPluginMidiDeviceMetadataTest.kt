package org.androidaudioplugin.hosting

import org.junit.Assert.assertEquals
import org.junit.Test
import org.kxml2.io.KXmlParser
import org.xmlpull.v1.XmlPullParser
import java.io.StringReader

class AudioPluginMidiDeviceMetadataTest {
    private fun parser(xml: String): XmlPullParser = KXmlParser().apply {
        setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES, true)
        setInput(StringReader(xml))
    }

    @Test
    fun midiDevicePortPluginIds() {
        val xml = """
            <devices xmlns:aap="urn:org.androidaudioplugin.core">
              <device name="ADL / OPN" manufacturer="example">
                <input-port name="ADL" aap:plugin-id="urn:adl" />
                <input-port name="Unmapped" />
                <output-port name="Out" aap:plugin-id="urn:ignored" />
                <input-port name="OPN" aap:plugin-id="urn:opn" />
              </device>
              <device name="ignored: a MIDI device service serves only the first device">
                <input-port name="X" aap:plugin-id="urn:x" />
              </device>
            </devices>
        """.trimIndent()
        assertEquals(listOf("urn:adl", null, "urn:opn"),
            AudioPluginMidiDeviceMetadata.readPortPluginIds(parser(xml), AudioPluginMidiDeviceMetadata.MIDI1_PORT_ELEMENT))
    }

    @Test
    fun umpDevicePortPluginIds() {
        val xml = """
            <devices xmlns:aap="urn:org.androidaudioplugin.core">
              <device name="ADL / OPN">
                <port name="ADL" aap:plugin-id="urn:adl" />
                <port name="OPN" aap:plugin-id="urn:opn" />
              </device>
            </devices>
        """.trimIndent()
        assertEquals(listOf("urn:adl", "urn:opn"),
            AudioPluginMidiDeviceMetadata.readPortPluginIds(parser(xml), AudioPluginMidiDeviceMetadata.UMP_PORT_ELEMENT))
    }
}
