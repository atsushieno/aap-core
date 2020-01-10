package org.androidaudioplugin.aaphostsample

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.os.IBinder
import android.util.Log
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.CompoundButton
import android.widget.Toast
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.audio_plugin_service_list_item.view.*
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.androidaudioplugin.*

class MainActivity : AppCompatActivity() {

    inner class PluginViewAdapter(ctx:Context, layout: Int, array: Array<Pair<AudioPluginServiceInformation,PluginInformation>>)
        : ArrayAdapter<Pair<AudioPluginServiceInformation,PluginInformation>>(ctx, layout, array)
    {
        fun processAudioOutputData()
        {
            var out_rawL = host.audioOutputs[0]
            var out_rawR = host.audioOutputs[1]
            out_raw = ByteArray(out_rawL.size + out_rawR.size)
            // merge L/R
            assert(out_rawL.size == out_rawR.size)
            for (i in 0 until out_rawL.size / 4) {
                out_raw[i * 8] = out_rawL[i * 4]
                out_raw[i * 8 + 1] = out_rawL[i * 4 + 1]
                out_raw[i * 8 + 2] = out_rawL[i * 4 + 2]
                out_raw[i * 8 + 3] = out_rawL[i * 4 + 3]
                out_raw[i * 8 + 4] = out_rawR[i * 4]
                out_raw[i * 8 + 5] = out_rawR[i * 4 + 1]
                out_raw[i * 8 + 6] = out_rawR[i * 4 + 2]
                out_raw[i * 8 + 7] = out_rawR[i * 4 + 3]
            }
        }

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
            view.audio_plugin_name.text = item.second.name
            view.audio_plugin_list_identifier.text = item.second.pluginId

            val service = item.first
            val plugin = item.second

            if (plugin.pluginId == null)
                throw UnsupportedOperationException ("Insufficient plugin information was returned; missing pluginId")

            view.plugin_toggle_switch.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->

                var intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
                intent.component = ComponentName(
                    service.packageName,
                    service.className
                )
                intent.putExtra("sampleRate", host.sampleRate)

                host.pluginInstantiatedListeners.clear()
                host.pluginInstantiatedListeners.add { instance ->
                    Log.d("MainActivity", "plugin instantiated listener invoked")

                    GlobalScope.launch {
                        Log.d("MainActivity", "starting AAPSampleInterop.runClientAAP")
                        prepareAudioData()
                        AAPSampleInterop.runClientAAP(instance.service.binder!!, host.sampleRate, plugin.pluginId!!,
                            host.audioInputs[0],
                            host.audioInputs[1],
                            host.audioOutputs[0],
                            host.audioOutputs[1])
                        processAudioOutputData()

                        runOnUiThread {
                            wavePostPlugin.sample = out_raw.map { b -> b.toInt() }.toIntArray()
                            Toast.makeText(this@MainActivity, "set output wav", Toast.LENGTH_LONG).show()
                        }
                        // FIXME: enable this line?
                        //host.unbindAudioPluginService("${service.packageName}/${service.className}")
                    }
                }
                host.instantiatePlugin(plugin)
            }

            return view
        }
    }

    lateinit var host : AudioPluginHost

    lateinit var in_raw : ByteArray
    lateinit var out_raw : ByteArray

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        host = AudioPluginHost(applicationContext)

        val wavAsset = assets.open("sample.wav")
        // read wave samples and and deinterleave into L/R
        in_raw = wavAsset.readBytes().drop(88).toByteArray() // skip WAV header (80 bytes for this file) and data chunk ID + size (8 bytes)
        wavAsset.close()

        Toast.makeText(this, "loaded input wav", Toast.LENGTH_LONG).show()

        setContentView(R.layout.activity_main)
        wavePrePlugin.sample = arrayOf(0).toIntArray() // dummy
        wavePostPlugin.sample = arrayOf(0).toIntArray() // dummy

        // Query AAPs
        val pluginServices = AudioPluginHostHelper.queryAudioPluginServices(this)
        val servicedPlugins = pluginServices.flatMap { s -> s.plugins.map { p -> Pair(s, p) } }.toTypedArray()

        val plugins = servicedPlugins.filter { p -> p.first.packageName != applicationInfo.packageName }.toTypedArray()
        val adapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, plugins)
        this.audioPluginServiceListView.adapter = adapter
        /*
        this.audioPluginServiceListView.setOnItemLongClickListener {parent, view, position, id ->
            val item = adapter.getItem(position)
            if (item == null)
                false
            val service = item!!.first
            val plugin = item!!.second

            var intent = Intent(AudioPluginHostHelper.AAP_ACTION_NAME)
            this.intent = intent
            intent.component = ComponentName(
                service.packageName,
                service.className
            )
            intent.putExtra("sampleRate", fixed_sample_rate)
            intent.putExtra("pluginId", plugin.pluginId)

            target_plugin = plugin.pluginId!!
            Log.d("MainActivity", "binding AudioPluginService: ${service.name} | ${service.packageName} | ${service.className}")
            bindService(intent, conn, Context.BIND_AUTO_CREATE)
            true
        }
         */

        playPrePluginLabel.setOnClickListener { GlobalScope.launch {playSound(host.sampleRate, false) } }
        playPostPluginLabel.setOnClickListener { GlobalScope.launch {playSound(host.sampleRate, true) } }

        out_raw = in_raw
        wavePrePlugin.sample = in_raw.map { b -> b.toInt() }.toIntArray()
        wavePrePlugin.requestLayout()
        wavePostPlugin.sample = in_raw.map { b -> b.toInt() }.toIntArray()
        wavePostPlugin.requestLayout()
    }

    fun prepareAudioData()
    {
        host.audioBufferSizeInBytes = in_raw.size / 2
        var in_rawL = host.audioInputs[0]
        var in_rawR = host.audioInputs[1]
        for (i in 0 until in_raw.size / 8) {
            in_rawL [i * 4] = in_raw[i * 8]
            in_rawL [i * 4 + 1] = in_raw[i * 8 + 1]
            in_rawL [i * 4 + 2] = in_raw[i * 8 + 2]
            in_rawL [i * 4 + 3] = in_raw[i * 8 + 3]
            in_rawR [i * 4] = in_raw[i * 8 + 4]
            in_rawR [i * 4 + 1] = in_raw[i * 8 + 5]
            in_rawR [i * 4 + 2] = in_raw[i * 8 + 6]
            in_rawR [i * 4 + 3] = in_raw[i * 8 + 7]
        }
    }

    fun playSound(sampleRate: Int, postApplied: Boolean)
    {
        val w = if(postApplied) out_raw else in_raw

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
        var fa = FloatArray(w.size / 4)
        for (i in 0 .. fa.size - 1) {
            val bits = (w[i * 4 + 3].toUByte().toInt() shl 24) + (w[i * 4 + 2].toUByte().toInt() shl 16) + (w[i * 4 + 1].toUByte().toInt() shl 8) + w[i * 4].toUByte().toInt()
            fa[i] = Float.fromBits(bits.toInt())
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
