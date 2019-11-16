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
import kotlinx.android.synthetic.main.activity_main.*
import org.androidaudioplugin.AudioPluginHost
import org.androidaudioplugin.AudioPluginServiceInformation
import org.androidaudioplugin.PluginInformation
import kotlinx.android.synthetic.main.audio_plugin_service_list_item.view.*
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.androidaudioplugin.lv2.AudioPluginLV2LocalHost

class MainActivity : AppCompatActivity() {

    inner class PluginViewAdapter(ctx:Context, layout: Int, isOutProcess: Boolean, array: Array<Pair<AudioPluginServiceInformation,PluginInformation>>)
        : ArrayAdapter<Pair<AudioPluginServiceInformation,PluginInformation>>(ctx, layout, array)
    {
        var intent: Intent? = null
        val isOutProcess: Boolean = isOutProcess

        lateinit var target_plugin: String

        fun disconnect() {
            Log.d("MainActivity", "disconnect invoked")
            GlobalScope.launch {
                context.unbindService(conn)
                intent = null
            }
        }

        var conn : ServiceConnection = object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
                Log.d("MainActivity", "onServiceConnected is invoked")

                if (binder == null)
                    throw UnsupportedOperationException ("binder must not be null")

                GlobalScope.launch {

                    Log.d("MainActivity", "starting AAPSampleInterop.initialize")
                    AAPSampleInterop.initialize(AudioPluginHost.queryAudioPluginServices(context).flatMap { s -> s.plugins }.toTypedArray())

                    Log.d("MainActivity", "starting AAPSampleInterop.runClientAAP")
                    AAPSampleInterop.runClientAAP(binder, fixed_sample_rate, target_plugin, in_rawL, in_rawR, out_rawL, out_rawR)

                    context.applicationContext.run {
                        // FIXME: merge L/R
                        wavePostPlugin.setRawData(out_rawL, {})
                        wavePostPlugin.progress = 100f
                    }
                    disconnect()
                }
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                Log.d("MainActivity", "onServiceDisconnected is invoked, most likely an unexpected crash?")
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
            view.audio_plugin_service_name.text = item.first.name
            view.audio_plugin_name.text = item.second.name
            view.audio_plugin_list_identifier.text = item.second.pluginId

            val service = item.first
            val plugin = item.second

            if (plugin.pluginId == null)
                throw UnsupportedOperationException ("Insufficient plugin information was returned; missing pluginId")

            view.plugin_toggle_switch.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->

                var intent = Intent(AudioPluginHost.AAP_ACTION_NAME)
                this.intent = intent
                intent.component = ComponentName(
                    service.packageName,
                    service.className
                )
                intent.putExtra("sampleRate", fixed_sample_rate)
                intent.putExtra("pluginId", plugin.pluginId)

                target_plugin = plugin.pluginId!!
                Log.d("MainActivity", "binding AudioPluginService: ${service.name} | ${service.packageName} | ${service.className}")
                context.bindService(intent, conn, Context.BIND_AUTO_CREATE)
            }

            return view
        }
    }

    // FIXME: this should be customizible, depending on the device configuration (sample input should be decoded appropriately).
    val fixed_sample_rate = 44100
    lateinit var in_rawL: ByteArray
    lateinit var in_rawR: ByteArray
    lateinit var out_rawL: ByteArray
    lateinit var out_rawR: ByteArray

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val wavAsset = assets.open("sample.wav")
        in_rawL = wavAsset.readBytes().drop(88).toByteArray() // skip WAV header (80 bytes for this file) and data chunk ID + size (8 bytes)
        wavAsset.close()
        in_rawR = in_rawL.clone()
        out_rawL = ByteArray(in_rawL.size)
        out_rawR = ByteArray(in_rawL.size)

        // Query AAPs
        val pluginServices = AudioPluginHost.queryAudioPluginServices(this)
        val servicedPlugins = pluginServices.flatMap { s -> s.plugins.map { p -> Pair(s, p) } }.toTypedArray()

        val remotePlugins = servicedPlugins.filter { p -> p.first.packageName != applicationInfo.packageName }.toTypedArray()
        val remoteAdapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, true, remotePlugins)
        this.audioPluginServiceListView.adapter = remoteAdapter

        playPrePluginLabel.setOnClickListener { GlobalScope.launch {playSound(fixed_sample_rate, false) } }
        playPostPluginLabel.setOnClickListener { GlobalScope.launch {playSound(fixed_sample_rate, true) } }

        wavePrePlugin.setRawData(in_rawL, {})
        wavePrePlugin.progress = 100f
    }

    fun playSound(sampleRate: Int, postApplied: Boolean)
    {
        val w = if(postApplied) out_rawL else in_rawL
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
