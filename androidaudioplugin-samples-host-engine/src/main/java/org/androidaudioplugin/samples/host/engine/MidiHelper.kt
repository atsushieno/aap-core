package org.androidaudioplugin.samples.host.engine

import dev.atsushieno.ktmidi.write

class MidiHelper {
    companion object {
        // see aap-midi2.h for MidiBufferHeader structure.
        internal fun toMidiBufferHeader(timeOption: Int, size: UInt): List<Byte> {
            // it's not really a UMP but reuse it for encoding to bytes...
            return timeOption.toPlatformNativeBytes() + size.toInt().toPlatformNativeBytes() +
                List(24) { 0 }
        }

        private fun Int.toPlatformNativeBytes() = listOf(this % 0x100, this / 0x100 % 0x100,
            this / 0x10000 % 0x100, this / 0x1000000).map { it.toByte() }
    }
}
