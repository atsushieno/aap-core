package org.androidaudioplugin.hosting

import org.xmlpull.v1.XmlPullParser

/**
 * Reads the AAP-specific parts of a MIDI device XML resource (`midi_device_info.xml` for
 * MidiDeviceService, `ump_device_info.xml` for MidiUmpDeviceService): the explicit mapping from
 * each port to the plugin that receives its messages.
 *
 * ```
 * <devices xmlns:aap="urn:org.androidaudioplugin.core">
 *   <device name="..." ...>
 *     <input-port name="..." aap:plugin-id="urn:some-plugin-id" />
 *   </device>
 * </devices>
 * ```
 *
 * The platform keeps only the `name` of each port, so AAP reads the resource by itself.
 */
object AudioPluginMidiDeviceMetadata {
    const val PLUGIN_ID_ATTRIBUTE = "plugin-id"
    /** The port element name in MIDI 1.0 device metadata (MidiDeviceService). */
    const val MIDI1_PORT_ELEMENT = "input-port"
    /** The port element name in UMP device metadata (MidiUmpDeviceService). */
    const val UMP_PORT_ELEMENT = "port"

    /**
     * Returns the `aap:plugin-id` of each port in declaration order (null where it is absent).
     * Only the first `<device>` is read: a MIDI device service component serves only one device.
     */
    @JvmStatic
    fun readPortPluginIds(xp: XmlPullParser, portElementName: String): List<String?> {
        val ret = mutableListOf<String?>()
        var inDevice = false
        while (true) {
            val eventType = xp.next()
            if (eventType == XmlPullParser.END_DOCUMENT)
                break
            if (eventType == XmlPullParser.START_TAG) {
                if (xp.name == "device")
                    inDevice = true
                else if (inDevice && xp.name == portElementName)
                    ret.add(xp.getAttributeValue(AudioPluginHostHelper.AAP_METADATA_CORE_NS, PLUGIN_ID_ATTRIBUTE)
                        ?: xp.getAttributeValue(null, PLUGIN_ID_ATTRIBUTE))
            } else if (eventType == XmlPullParser.END_TAG && xp.name == "device")
                break
        }
        return ret
    }
}
