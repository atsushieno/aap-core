package org.androidaudioplugin

/**
 * Port information structure. The members mostly correspond to attributes in a `<port>` element in
 * `aap_metadata.xml`.
 */
class PortInformation(var index: Int, var name: String, var direction: Int, var content: Int)
{
    companion object {
        const val PORT_DIRECTION_INPUT = 0
        const val PORT_DIRECTION_OUTPUT = 1

        // Open-end list of content characteristics i.e. it can be extended in the future (reserved)
        const val PORT_CONTENT_TYPE_GENERAL = 0
        const val PORT_CONTENT_TYPE_AUDIO = 1
        const val PORT_CONTENT_TYPE_MIDI = 2
        const val PORT_CONTENT_TYPE_MIDI2 = 3
    }

    var minimumSizeInBytes: Int = 0
}
