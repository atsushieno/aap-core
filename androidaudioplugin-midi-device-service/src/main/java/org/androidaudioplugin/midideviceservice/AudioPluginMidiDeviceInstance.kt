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

// Unlike MidiReceiver, it is instantiated whenever the port is opened, and disposed every time it is closed.
// By isolating most of the implementation here, it makes better lifetime management.
class AudioPluginMidiDeviceInstance private constructor(
    // It is used to manage Service connections, not instancing (which is managed by native code).
    private val client: AudioPluginClientBase) {

    companion object {
        /**
         * @param outputPortReceiver  [MidiReceiver] for an output port declared in the service's
         *   midi_device_info.xml, used to forward MIDI-CI response bytes to connected host
         *   applications.  May be null when no output port is declared; in that case CI responses
         *   are silently discarded, but the native CI session is still fully operational.
         */
        suspend fun create(pluginId: String, ownerService: AudioPluginMidiDevice,
                           midiTransport: Int,
                           outputPortReceiver: MidiReceiver? = null) : AudioPluginMidiDeviceInstance {
            val audioManager = ownerService.applicationContext.getSystemService(Context.AUDIO_SERVICE) as AudioManager
            val sampleRate = audioManager.getProperty(AudioManager.PROPERTY_OUTPUT_SAMPLE_RATE)?.toInt() ?: 44100
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

            // Always wire up the CI output sender so that setMidiOutputSender() is
            // unconditionally invoked on the native side.  If no output port was
            // declared (outputPortReceiver == null) the lambda is a no-op and CI
            // responses are silently discarded; the native guard `if (midi_output_sender)`
            // still fires correctly because the sender function object is non-empty.
            //
            // Pacing (inter-chunk sleep) is handled in C++ by the JNI sender via
            // std::this_thread::sleep_for(), so we forward bytes directly here without
            // any additional buffering or thread switching to preserve message ordering.
            ret.setMidiOutputCallback(MidiOutputCallback { data, offset, count, timestamp ->
                outputPortReceiver?.send(data, offset, count, timestamp)
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