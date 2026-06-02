package org.androidaudioplugin.midideviceservice

import android.content.Context
import android.media.AudioManager
import android.media.midi.MidiReceiver
import org.androidaudioplugin.hosting.AudioPluginClientBase

/**
 * Callback interface used to forward MIDI-CI response bytes from the native CI
 * session back to the connected Android MIDI host via an output port receiver.
 *
 * The callback is always registered after [AudioPluginMidiDeviceInstance.activate].
 * CI responses only reach the host if the service declares at least one output port
 * in midi_device_info.xml; otherwise the callback is a no-op and responses are
 * silently discarded.
 */
fun interface MidiOutputCallback {
    fun send(data: ByteArray, offset: Int, count: Int, timestamp: Long)
}

fun interface MidiOutputReceiverProvider {
    fun get(): MidiReceiver?
}

// Unlike MidiReceiver, it is instantiated whenever the port is opened, and disposed every time it is closed.
// By isolating most of the implementation here, it makes better lifetime management.
class AudioPluginMidiDeviceInstance private constructor(
    // It is used to manage Service connections, not instancing (which is managed by native code).
    private val client: AudioPluginClientBase) {

    companion object {
        /**
         * @param outputPortReceiverProvider supplies the [MidiReceiver] for an output port declared
         *   in the service's midi_device_info.xml. It may return null when the output side is not
         *   currently open, in which case CI responses are dropped.
         */
        suspend fun create(pluginId: String, ownerService: AudioPluginMidiDevice,
                           midiTransport: Int,
                           outputPortReceiverProvider: MidiOutputReceiverProvider) : AudioPluginMidiDeviceInstance {
            val audioManager = ownerService.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 48000
            val oboeFrameSize = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_FRAMES_PER_BUFFER)?.toInt() ?: 1024

            val client = AudioPluginClientBase(ownerService.applicationContext)

            val ret = AudioPluginMidiDeviceInstance(client)

            // FIXME: adjust audioOutChannelCount and appFrameSize somewhere?

            ret.initializeMidiProcessor(client.serviceConnectionId,
                sampleRate, oboeFrameSize, ret.audioOutChannelCount, ret.aapFrameSize,
                ret.midiBufferSize, midiTransport)

            val pluginInfo = ownerService.plugins.first { p -> p.pluginId == pluginId }
            client.connectToPluginService(pluginInfo.packageName)
            ret.instantiatePlugin(pluginId)
            ret.activate()

            ret.setMidiOutputCallback(MidiOutputCallback { data, offset, count, timestamp ->
                val out = outputPortReceiverProvider.get() ?: return@MidiOutputCallback
                out.send(data, offset, count, timestamp)
            })

            return ret
        }
    }

    private val audioOutChannelCount: Int = 2
    private val aapFrameSize = 512
    private val midiBufferSize = 4096

    fun onDeviceClosed() {
        setMidiOutputCallback(null)
        deactivate()
        client.dispose()
        terminateMidiProcessor()
    }

    fun onSend(msg: ByteArray?, offset: Int, count: Int, timestamp: Long) {
        // We skip too lengthy MIDI buffer.
        val actualSize = if (count > midiBufferSize) midiBufferSize else count

        processMessage(msg, offset, actualSize, timestamp)
    }

    // Initialize basic native parts, without any plugin information.
    private external fun initializeMidiProcessor(
        connectorInstanceId: Int,
        sampleRate: Int,
        oboeFrameSize: Int,
        audioOutChannelCount: Int,
        aapFrameSize: Int,
        midiBufferSize: Int,
        midiTransport: Int)
    private external fun terminateMidiProcessor()
    private external fun instantiatePlugin(pluginId: String)
    private external fun processMessage(msg: ByteArray?, offset: Int, count: Int, timestampInNanoseconds: Long)
    private external fun activate()
    private external fun deactivate()

    /**
     * Register (or unregister, when null) the native MIDI-CI output sender.
     * The callback's [MidiOutputCallback.send] is invoked on whichever thread the
     * CI session uses to dispatch responses (typically the Android MIDI receiver
     * thread).
     */
    external fun setMidiOutputCallback(callback: MidiOutputCallback?)
}
