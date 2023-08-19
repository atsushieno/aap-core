package org.androidaudioplugin.manager

import dev.atsushieno.ktmidi.Ump
import dev.atsushieno.ktmidi.UmpFactory
import dev.atsushieno.ktmidi.toPlatformNativeBytes
import org.androidaudioplugin.hosting.UmpHelper

class PluginPlayer private constructor(private val native: Long) : AutoCloseable {
    companion object {
        const val sample_audio_filename = "androidaudioplugin_manager_sample_audio.ogg"

        fun create() = PluginPlayer(createNewPluginPlayer())

        init {
            System.loadLibrary("androidaudioplugin-manager")
        }

        private external fun createNewPluginPlayer(): Long
    }

    override fun close() {
        deletePluginPlayer(native)
    }

    private external fun deletePluginPlayer(native: Long)

    fun loadAudioResource(bytes: ByteArray, filename: String) =
        loadAudioResourceNative(bytes, filename)

    private external fun loadAudioResourceNative(bytes: ByteArray, filename: String)

    fun setTargetInstance(instanceId: Int) = setTargetInstanceNative(native, instanceId)

    private external fun setTargetInstanceNative(native: Long, instanceId: Int)

    fun startProcessing() = startProcessingNative(native)

    fun pauseProcessing() = pauseProcessingNative(native)

    private external fun startProcessingNative(native: Long)

    private external fun pauseProcessingNative(native: Long)

    fun playPreloadedAudio() = playPreloadedAudioNative(native)

    private external fun playPreloadedAudioNative(native: Long)

    fun setParameterValue(parameterId: UInt, value: Float) {
        val umps = UmpHelper.aapUmpSysex8Parameter(parameterId, value).flatMap { Ump(it).toPlatformNativeBytes().asList() }
        addMidiEvents(umps.toByteArray())
    }

    fun addMidiEvent(ump: Long) = addMidiEvent(Ump(ump))

    fun addMidiEvent(ump: Ump) = addMidiEvents(ump.toPlatformNativeBytes())

    fun addMidiEvents(bytes: ByteArray, offset: Int = 0, length: Int = bytes.size - offset) =
        addMidiEventNative(native, bytes, offset, length)

    private external fun addMidiEventNative(native: Long, bytes: ByteArray, offset: Int = 0, length: Int = bytes.size - offset)

    fun processPitchBend(note: Int, value: Float) {
        assert(0.0 <= value && value < 1.0)
        val ump =
            if (note < 0) UmpFactory.midi2PitchBend(0, 0, (0x1_0000_0000 * value).toLong())
            else UmpFactory.midi2PerNotePitchBend(0, 0, note, (0x1_0000_0000 * value).toLong())
        addMidiEvent(ump)
    }

    fun processPressure(note: Int, value: Float) {
        assert(0.0 <= value && value < 1.0)
        val ump = Ump(UmpFactory.midi2PAf(0, 0, note, (0x1_0000_0000 * value).toLong()))
        addMidiEvent(ump)
    }

    fun setNoteState(note: Int, velocity16: Int, isNoteOn: Boolean) {
        val ump =
            if (isNoteOn) Ump(UmpFactory.midi2NoteOn(0, 0, note, 0, velocity16, 0))
            else Ump(UmpFactory.midi2NoteOff(0, 0, note, 0, velocity16, 0))
        addMidiEvent(ump)
    }
}
