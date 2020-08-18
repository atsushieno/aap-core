package org.androidaudioplugin.aaphostsample

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.Window
import android.widget.ArrayAdapter
import android.widget.CompoundButton
import android.widget.SeekBar
import android.widget.Toast
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.audio_plugin_parameters_list_item.view.*
import kotlinx.android.synthetic.main.audio_plugin_service_list_item.view.*
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.yield
import org.androidaudioplugin.*
import java.lang.Exception
import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.time.ExperimentalTime
import kotlin.time.measureTime

@ExperimentalUnsignedTypes
class MainActivity : AppCompatActivity() {

    inner class PluginViewAdapter(ctx:Context, layout: Int, array: Array<Pair<AudioPluginServiceInformation,PluginInformation>>)
        : ArrayAdapter<Pair<AudioPluginServiceInformation,PluginInformation>>(ctx, layout, array)
    {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            val item = getItem(position)
            var view = convertView
            if (view == null)
                view = LayoutInflater.from(context).inflate(R.layout.audio_plugin_service_list_item, parent, false)
            if (view == null)
                throw UnsupportedOperationException()
            if (item == null)
                return view
            view.audio_plugin_service_name.text = item.first.label
            view.audio_plugin_name.text = item.second.displayName
            view.audio_plugin_list_identifier.text = item.second.pluginId

            val service = item.first
            val plugin = item.second

            if (plugin.pluginId == null)
                throw UnsupportedOperationException ("Insufficient plugin information was returned; missing pluginId")

            view.setOnClickListener {
                portsAdapter = PortViewAdapter(this@MainActivity, R.layout.audio_plugin_parameters_list_item, plugin.ports)
                this@MainActivity.audioPluginParametersListView.adapter = portsAdapter
                this@MainActivity.selectedPluginIndex = position
            }

            return view
        }
    }

    inner class PortViewAdapter(ctx:Context, layout: Int, private var ports: List<PortInformation>)
        : ArrayAdapter<PortInformation>(ctx, layout, ports)
    {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View
        {
            val item = getItem(position)
            var view = convertView
            if (view == null)
                view = LayoutInflater.from(context).inflate(R.layout.audio_plugin_parameters_list_item, parent, false)
            if (view == null)
                throw UnsupportedOperationException()
            if (item == null)
                return view

            view.audio_plugin_parameter_content_type.text = if(item.content == 1) "Audio" else if(item.content == 2) "Midi" else "Other"
            view.audio_plugin_parameter_direction.text = if(item.direction == 0) "In" else "Out"
            view.audio_plugin_parameter_name.text = item.name
            view.audio_plugin_seekbar_parameter_value.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    parameters[position] = progress / 100.0f
                    view.audio_plugin_edit_text_parameter_value.text.clear()
                    view.audio_plugin_edit_text_parameter_value.text.insert(0, parameters[position].toString())
                }
                override fun onStartTrackingTouch(seekBar: SeekBar?) {
                }

                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                }
            })
            var value = parameters[ports.indexOf(item)]
            view.audio_plugin_seekbar_parameter_value.progress = (100.0 * value).toInt()

            return view
        }

        val parameters = ports.map { p -> p.default }.toFloatArray()
    }

    private lateinit var host : AudioPluginHost
    private var instance : AudioPluginInstance? = null

    private lateinit var inBuf : ByteArray
    private lateinit var outBuf : ByteArray
    private var portsAdapter : PortViewAdapter? = null
    private var selectedPluginIndex : Int = -1

    override fun onDestroy() {
        super.onDestroy()
        host.dispose()
    }

    @ExperimentalTime
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestWindowFeature(Window.FEATURE_NO_TITLE)

        host = AudioPluginHost(applicationContext)

        val wavAsset = assets.open("sample.wav")
        // read wave samples and and deinterleave into L/R
        inBuf = wavAsset.readBytes().drop(88).toByteArray() // skip WAV header (80 bytes for this file) and data chunk ID + size (8 bytes)
        wavAsset.close()

        Toast.makeText(this, "loaded input wav", Toast.LENGTH_LONG).show()

        setContentView(R.layout.activity_main)
        wavePostPlugin.sample = arrayOf(0).toIntArray() // dummy

        // Query AAPs
        val pluginServices = AudioPluginHostHelper.queryAudioPluginServices(this)
        val servicedPlugins = pluginServices.flatMap { s -> s.plugins.map { p -> Pair(s, p) } }.toTypedArray()

        val plugins = servicedPlugins.filter { p -> p.first.packageName != applicationInfo.packageName }.toTypedArray()
        val adapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, plugins)
        this.audioPluginListView.adapter = adapter


        applyToggleButton.setOnCheckedChangeListener { _, checked ->
            if (checked) {
                if (selectedPluginIndex < 0)
                    return@setOnCheckedChangeListener
                var item = adapter.getItem(selectedPluginIndex)
                if (item != null)
                    applyPlugin(item.first, item.second)
            } else {
                wavePostPlugin.sample = inBuf.map { b -> b.toInt() }.toIntArray()
                wavePostPlugin.requestLayout()
            }
        }
        playPostPluginLabel.setOnClickListener { GlobalScope.launch {playSound(host.sampleRate, applyToggleButton.isChecked) } }

        outBuf = ByteArray(inBuf.size)
        wavePostPlugin.sample = inBuf.map { b -> b.toInt() }.toIntArray()
        wavePostPlugin.requestLayout()
    }

    @ExperimentalTime
    private fun applyPlugin(service: AudioPluginServiceInformation, plugin: PluginInformation)
    {
        val intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
        intent.component = ComponentName(
            service.packageName,
            service.className
        )
        intent.putExtra("sampleRate", host.sampleRate)

        host.pluginInstantiatedListeners.clear()
        host.pluginInstantiatedListeners.add { instance ->
            this.instance = instance
            var floatCount = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)
            instance.prepare(host.sampleRate, floatCount)

            GlobalScope.launch {
                processPluginOnce()
            }
        }
        host.instantiatePlugin(plugin)

    }

    var PROCESS_AUDIO_NATIVE = false

    var FRAMES_PER_TICK = 100
    var FRAMES_PER_SECOND = 44100

    private fun getFixedMidiEventLength(seq: UByteArray, from: Int) =
        when (seq[from].toUInt().toInt() and 0xF0) {
            0xA0, 0xC0 -> 2
            0xF0 ->
                when (seq[from].toUInt().toInt()) {
                    0xF1, 0xF3 -> 1
                    0xF2 -> 2
                    else -> 0
                }
            else -> 3
        }

    private fun splitMidiEvents(seq: UByteArray) = sequence {
        var cur = 0
        var i = 0
        while (i < seq.size) {
            // delta time
            while (seq[i] >= 0x80u)
                i++
            i++
            // message
            var evPos = i
            if (seq[evPos] == 0xF0u.toUByte())
                i += seq.drop(i).indexOf(0xF7u) + 1
            else
                i += getFixedMidiEventLength(seq, i)
            yield(seq.slice(IntRange(cur, i - 1)))
            cur = i
        }
    }

    private fun groupMidiEventsByTiming(events: Sequence<List<UByte>>) = sequence {
        var i = 0
        var iter = events.iterator()
        var list = mutableListOf<List<UByte>>()
        while (iter.hasNext()) {
            var ev = iter.next()
            if (getFirstMidiEventDuration(ev) != 0) {
                if (list.any())
                    yield(list.toList())
                list = mutableListOf<List<UByte>>()
            }
            list.add(ev)
        }
        if (list.any())
            yield(list.toList())
    }

    private fun getFirstMidiEventDuration(bytes: List<UByte>) : Int {
        var len = 0
        var pos = 0
        var mul = 1
        while (pos < bytes.size) {
            var b = bytes[pos]
            len += (b.toInt() and 0x7F) * mul
            pos++
            if (b < 0x80u)
                break
            mul *= 0x80
        }
        return len
    }

    private fun Int.toBCD() = (this / 10) shl 4 + (this % 10)

    private fun Int.fromBCD() = ((this and 0xF0) shr 4) * 10 + (this and 0xF)

    private fun toMidiTimeCode(frameRate: Int, framesPerSeconds: Int, frames: Int) : Array<UByte> {
        var frameUnit = framesPerSeconds / frameRate
        var seconds = frames / framesPerSeconds
        var ticks = ((frames % framesPerSeconds) / frameUnit)
        var ret = arrayOf(ticks.toUByte(),
            (seconds % 60).toBCD().toUByte(),
            (seconds / 60).toBCD().toUByte(),
            (seconds / 3600).toBCD().toUByte())
        return ret
    }

    private fun expandSMPTE(frameRate: Int, framesPerSeconds: Int, smpte: Int) : Int {
        var ticks = smpte and 0xFF
        var seconds = (smpte shr 8 and 0xFF).fromBCD()
        var minutes = (smpte shr 16 and 0xFF).fromBCD()
        var hours = (smpte shr 24 and 0xFF).fromBCD()
        return (hours * 3600 + minutes * 60 + seconds) * framesPerSeconds + ticks * frameRate
    }

    @ExperimentalTime
    private fun processPluginOnce() {
        var instance = this.instance!!
        var plugin = instance.pluginInfo
        var parameters =
            (0 until plugin.ports.count()).map { i -> plugin.ports[i].default }.toFloatArray()
        var a = this@MainActivity.portsAdapter
        if (a != null)
            parameters = a.parameters

        // Maybe we should simply use ktmidi API from fluidsynth-midi-service-j repo ...
        var noteOnSeq = arrayOf(
                110, 0x90, 0x39, 0x78, // 1 tick = 100 frames (FRAMES_PER_TICK), 110 ticks = 11000 frames
                0, 0x90, 0x3D, 0x78,
                0, 0x90, 0x40, 0x78)
                .map {i -> i.toUByte() }
        var noteOffSeq = arrayOf(
                110, 0x80, 0x39, 0,
                0, 0x80, 0x3D, 0,
                0, 0x80, 0x40, 0)
                .map {i -> i.toUByte() }
        var noteSeq = noteOnSeq.plus(noteOffSeq)
        var midiSeq = noteSeq
        repeat(9, { midiSeq += noteSeq}) // repeat 10 times
        var midiEvents = splitMidiEvents(midiSeq.toUByteArray())
        var midiEventsGroups = groupMidiEventsByTiming(midiEvents).toList()

        // Kotlin version of audio/MIDI processing.
        if (!PROCESS_AUDIO_NATIVE) {

            var audioInL = -1
            var audioInR = -1
            var audioOutL = -1
            var audioOutR = -1
            var midiIn = -1
            instance.pluginInfo.ports.forEachIndexed { i, p ->
                if (p.content == PortInformation.PORT_CONTENT_TYPE_AUDIO) {
                    if (p.direction == PortInformation.PORT_DIRECTION_INPUT) {
                        if (audioInL < 0)
                            audioInL = i
                        else
                            audioInR = i
                    } else {
                        if (audioOutL < 0)
                            audioOutL = i
                        else
                            audioOutR = i
                    }
                } else if (p.content == PortInformation.PORT_CONTENT_TYPE_MIDI) {
                    if (p.direction == PortInformation.PORT_DIRECTION_INPUT)
                        midiIn = i
                }
            }

            var bufferFrameSize = host.audioBufferSizeInBytes / 4 // 4 is sizeof(float)

            (0 until plugin.ports.count()).map { i ->
                if (plugin.ports[i].direction == PortInformation.PORT_DIRECTION_OUTPUT)
                    return@map
                if (plugin.ports[i].content == PortInformation.PORT_CONTENT_TYPE_AUDIO)
                    return@map
                if (plugin.ports[i].content == PortInformation.PORT_CONTENT_TYPE_MIDI)
                    return@map
                else {
                    var c = instance.getPortBuffer(i).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer()
                    c.position(0)
                    var v = parameters[i]
                    (0 until bufferFrameSize).forEach { c.put(v) }
                }
            }

            instance.activate()

            var currentFrame = 0
            var midiEventsGroupsIterator = midiEventsGroups.iterator()
            var nextMidiGroup = if (midiEventsGroupsIterator.hasNext()) midiEventsGroupsIterator.next() else listOf<List<UByte>>()
            var nextMidiEventDeltaTime = if (nextMidiGroup.size > 0) getFirstMidiEventDuration(nextMidiGroup.first()) else 0
            var nextMidiEventFrame = expandSMPTE(FRAMES_PER_TICK, FRAMES_PER_SECOND, nextMidiEventDeltaTime)

            // We process audio and MIDI buffers in this loop, until currentFrame reaches the end of
            // the input sample data. Note that it does not involve any real tiem processing.
            // For such usage, there should be native audio callback based on Oboe or AAudio
            // (not in Kotlin).
            while (currentFrame < inBuf.size / 2 / 4) { // channels / sizeof(float)
                deinterleaveInput(currentFrame, bufferFrameSize)

                if (audioInL >= 0) {
                    var instanceInL = instance.getPortBuffer(audioInL)
                    instanceInL.position(0)
                    instanceInL.put(host.audioInputs[0], 0, bufferFrameSize * 4)
                    if (audioInR > audioInL) {
                        var instanceInR = instance.getPortBuffer(audioInR)
                        instanceInR.position(0)
                        instanceInR.put(host.audioInputs[1], 0, bufferFrameSize * 4)
                    }
                }
                if (midiIn >= 0) {
                    // MIDI buffer is complicated. The AAP input MIDI buffer is formed as follows:
                    // - i32 length unit specifier: positive frames, or negative frames per beat in the
                    //   context tempo.
                    // - i32 MIDI buffer size
                    // - MIDI buffer contents in SMF-compatible format (but split in audio buffer)

                    var midiBuffer = instance.getPortBuffer(midiIn).order(ByteOrder.LITTLE_ENDIAN)
                    midiBuffer.position(0)
                    midiBuffer.position(resetMidiBuffer(midiBuffer))
                    var midiDataLengthInLoop = 0

                    while (nextMidiEventFrame < currentFrame + bufferFrameSize) {
                        // FIXME: adjust delta time, the position is practically ignored now.
                        var timedEvent = nextMidiGroup.first()
                        var deltaTimeTmp = getFirstMidiEventDuration(timedEvent)
                        var deltaTimeBytes = 1
                        while (deltaTimeTmp > 0x80) {
                            deltaTimeBytes++
                            deltaTimeTmp /= 0x80
                        }
                        var diffFrame = nextMidiEventFrame % bufferFrameSize
                        var diffMTC = toMidiTimeCode(FRAMES_PER_TICK, FRAMES_PER_SECOND, diffFrame)
                        var b0 = 0.toUByte()
                        var diffMTCLength = if (diffMTC[3] != b0) 4 else if (diffMTC[2] != b0) 3 else if (diffMTC[1] != b0) 2 else 1
                        var updatedFirstEvent = diffMTC.take(diffMTCLength).plus(timedEvent.drop(deltaTimeBytes))
                        var nextMidiEvents = updatedFirstEvent.plus(nextMidiGroup.drop(1).flatten()).map { u -> u.toByte() }.toByteArray()
                        midiDataLengthInLoop += nextMidiEvents.size
                        midiBuffer.put(nextMidiEvents)
                        if (!midiEventsGroupsIterator.hasNext())
                            break
                        nextMidiGroup = midiEventsGroupsIterator.next()
                        nextMidiEventFrame += expandSMPTE(FRAMES_PER_TICK, FRAMES_PER_SECOND, getFirstMidiEventDuration(nextMidiGroup.first()))
                    }
                    var xz = midiBuffer.position()
                    midiBuffer.position(4)
                    midiBuffer.putInt(midiDataLengthInLoop)
                }

                var duration = measureTime {
                    instance.process()
                }
                //Log.d("AAPHost Perf", "instance process time: " + duration.inNanoseconds)

                if (audioOutL >= 0) {
                    var instanceOutL = instance.getPortBuffer(audioOutL)
                    instanceOutL.position(0)
                    instanceOutL.get(host.audioOutputs[0], 0, bufferFrameSize * 4)
                    if (audioOutR > audioOutL) {
                        var instanceOutR = instance.getPortBuffer(audioOutR)
                        instanceOutR.position(0)
                        instanceOutR.get(host.audioOutputs[1], 0, bufferFrameSize * 4)
                    } else {
                        // monoral output
                        instanceOutL.position(0)
                        instanceOutL.get(host.audioOutputs[1], 0, bufferFrameSize * 4)
                    }
                }

                interleaveOutput(currentFrame, bufferFrameSize)

                currentFrame += bufferFrameSize
            }

            instance.deactivate()

        } else {
            // Native version of audio processing. (Note that it passes the entire wav buffer.)
            host.audioBufferSizeInBytes = inBuf.size / 2 // 2 is for stereo

            deinterleaveInput(0, host.audioInputs[0].size / 4) // 4 is sizeof(float)
            AAPSampleInterop.runClientAAP(
                instance!!.service.binder!!, host.sampleRate, plugin.pluginId!!,
                host.audioInputs[0],
                host.audioInputs[1],
                host.audioOutputs[0],
                host.audioOutputs[1],
                parameters
            )
            interleaveOutput(0, host.audioOutputs[0].size / 4) // 4 is sizeof(float)
        }

        onProcessAudioCompleted()
    }

    private fun resetMidiBuffer(mb: ByteBuffer) : Int
    {
        var mbi = mb.asIntBuffer()
        var ticksPerFrame : Short = (-1 * FRAMES_PER_TICK).toShort()
        mbi.put(ticksPerFrame.toInt()) // 1 frame = 10 milliseconds
        mbi.put(0)

        return 8
    }

    private fun onProcessAudioCompleted()
    {
        runOnUiThread {
            wavePostPlugin.sample = outBuf.map { b -> b.toInt() }.toIntArray()
            Toast.makeText(this@MainActivity, "set output wav", Toast.LENGTH_LONG).show()
        }
        var instance = this.instance
        if (instance != null) {
            var serviceInfo = instance.service.serviceInfo
            host.unbindAudioPluginService(serviceInfo.packageName, serviceInfo.className)
        }
    }

    private fun deinterleaveInput(startInFloat: Int, sizeInFloat: Int)
    {
        val l = ByteBuffer.wrap(host.audioInputs[0]).asFloatBuffer()
        val r = ByteBuffer.wrap(host.audioInputs[1]).asFloatBuffer()
        var inF = ByteBuffer.wrap(inBuf).asFloatBuffer()
        inF.position(startInFloat * 2)
        var j = startInFloat * 2
        var end = inBuf.size / 4
        for (i in 0 until sizeInFloat) {
            if (j >= end)
                break
            l.put(inF[j++])
            r.put(inF[j++])
        }
    }

    private fun interleaveOutput(startInFloat: Int, sizeInFloat: Int)
    {
        val outL = ByteBuffer.wrap(host.audioOutputs[0]).asFloatBuffer()
        val outR = ByteBuffer.wrap(host.audioOutputs[1]).asFloatBuffer()
        var outF = ByteBuffer.wrap(outBuf).asFloatBuffer()
        outF.position(startInFloat * 2)
        var j = startInFloat * 2
        var end = outBuf.size / 4
        for (i in 0 until sizeInFloat) {
            if (j >= end)
                break
            outF.put(outL[i])
            j++
            outF.put(outR[i])
            j++
        }
    }

    private fun playSound(sampleRate: Int, postApplied: Boolean)
    {
        val w = if(postApplied) outBuf else inBuf

        val bufferSize = AudioTrack.getMinBufferSize(sampleRate, AudioFormat.CHANNEL_OUT_STEREO, AudioFormat.ENCODING_PCM_FLOAT)
        val track = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build())
            .setAudioFormat(AudioFormat.Builder()
                .setEncoding(AudioFormat.ENCODING_PCM_FLOAT)
                .setSampleRate(sampleRate)
                .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                .build())
            .setBufferSizeInBytes(bufferSize)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        track.setVolume(5.0f)
        track.play()
        var i = 0
        /* It is super annoying... there is no way to convert ByteBuffer to little-endian float array. I ended up to convert it manually here */
        val fa = FloatArray(w.size / 4)
        for (i in 0 .. fa.size - 1) {
            val bits = (w[i * 4 + 3].toUByte().toInt() shl 24) + (w[i * 4 + 2].toUByte().toInt() shl 16) + (w[i * 4 + 1].toUByte().toInt() shl 8) + w[i * 4].toUByte().toInt()
            fa[i] = Float.fromBits(bits)
        }
        while (i < w.size) {
            val ret = track.write(fa, i, bufferSize / 4, AudioTrack.WRITE_BLOCKING)
            if (ret <= 0)
                break
            i += ret
        }
        track.flush()
        track.stop()
        track.release()
    }
}
