package org.androidaudioplugin

class PortInformation(var name: String, var direction: Int, var content: Int, defaultValue: Float = 0.0f, minimumValue: Float = 0.0f, maximumValue: Float = 1.0f)
{
    companion object {
        const val PORT_DIRECTION_INPUT = 0
        const val PORT_DIRECTION_OUTPUT = 1

        // Open-end list of content characteristics i.e. it can be extended in the future (reserved)
        const val PORT_CONTENT_TYPE_GENERAL = 0
        const val PORT_CONTENT_TYPE_AUDIO = 1
        const val PORT_CONTENT_TYPE_MIDI = 2
    }

    var hasValueRange : Boolean = false

    var default: Float = defaultValue
        set(value: Float) {
            hasValueRange = true
            field = value
        }
    var minimum: Float = minimumValue
        set(value: Float) {
            hasValueRange = true
            field = value
        }

    var maximum: Float = maximumValue
        set(value: Float) {
            hasValueRange = true
            field = value
        }
}