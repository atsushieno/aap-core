package org.androidaudioplugin

// Bus information of a configured plugin instance. `getPortIndex()` resolves the backing port buffer.
class BusInformation(val id: Int, val kind: Int, val direction: Int, val role: Int,
                     val name: String, val layout: String, val portIndices: IntArray) {
    companion object {
        const val BUS_KIND_AUDIO = 1
        const val BUS_KIND_EVENT = 2

        const val BUS_ROLE_MAIN = 0
        const val BUS_ROLE_AUX = 1

        // flags for the buses changed listener
        const val BUSES_CHANGED_NAMES = 1
        const val BUSES_CHANGED_LAYOUT = 2
    }

    // The number of audio channels. It is 0 for event buses.
    val channelCount: Int
        get() = if (kind == BUS_KIND_AUDIO) portIndices.size else 0

    // The port index of the audio channel, or the event buffer (channel 0). -1 if out of range.
    fun getPortIndex(channel: Int = 0) = portIndices.getOrElse(channel) { -1 }
}
