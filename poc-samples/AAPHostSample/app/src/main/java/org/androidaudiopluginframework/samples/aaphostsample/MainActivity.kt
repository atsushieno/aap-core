package org.androidaudiopluginframework.samples.aaphostsample

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
import org.androidaudiopluginframework.AudioPluginHost
import org.androidaudiopluginframework.hosting.AAPLV2Host
import org.androidaudiopluginframework.AudioPluginServiceInformation
import org.androidaudiopluginframework.PluginInformation
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.android.synthetic.main.audio_plugin_service_list_item.view.*
import org.androidaudiopluginframework.samples.LV2PluginsInThisApp
import java.nio.ByteBuffer
import java.nio.FloatBuffer

class MainActivity : AppCompatActivity() {

    inner class PluginViewAdapter(ctx:Context, layout: Int, isOutProcess: Boolean, array: Array<Pair<AudioPluginServiceInformation,PluginInformation>>)
        : ArrayAdapter<Pair<AudioPluginServiceInformation,PluginInformation>>(ctx, layout, array)
    {
        var intent: Intent? = null
        val isOutProcess: Boolean = isOutProcess

        var conn = object : ServiceConnection {
            override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
                Log.i("HostMainActivity", "onServiceConnected invoked")
            }

            override fun onServiceDisconnected(name: ComponentName?) {
                Log.i("HostMainActivity", "onServiceDisconnected invoked")
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
            view.audio_plugin_list_identifier.text = item.second.uniqueId

            val service = item.first
            val plugin = item.second

            if (isOutProcess) {
                view.plugin_toggle_switch.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
                    if (intent != null) {
                        Log.i("Stopping ", "${service.name} | ${service.packageName} | ${service.className}")
                        context.stopService(intent)
                        context.unbindService(conn)
                        intent = null
                    } else {
                        Log.i("Instantiating ", "${service.name} | ${service.packageName} | ${service.className}")

                        intent = Intent(AudioPluginHost.AAP_ACTION_NAME)
                        intent!!.component = ComponentName(
                            service.packageName,
                            service.className
                        )

                        context.bindService(intent, conn, Context.BIND_AUTO_CREATE)
                        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O)
                            context.startForegroundService(intent)
                        else
                            context.startService(intent)
                    }
                }
            } else {
                view.plugin_toggle_switch.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
                    val uri = item.second.uniqueId?.substring("lv2:".length)
                    AAPLV2Host.runHost(LV2PluginsInThisApp.lv2Paths, this@MainActivity.assets, arrayOf(uri!!), in_raw, out_raw)
                    wavePostPlugin.setRawData(out_raw, {})
                    wavePostPlugin.progress = 100f
                }
            }

            return view
        }
    }

    lateinit var in_raw: ByteArray
    lateinit var out_raw: ByteArray

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        val wavAsset = assets.open("sample.wav")
        in_raw = wavAsset.readBytes().drop(88).toByteArray() // skip WAV header (80 bytes for this file) and data chunk ID + size (8 bytes)
        wavAsset.close()
        out_raw = ByteArray(in_raw.size)

        // Query AAPs
        val pluginServices = AudioPluginHost().queryAudioPluginServices(this)
        val servicedPlugins = pluginServices.flatMap { s -> s.plugins.map { p -> Pair(s, p) } }.toTypedArray()

        val servicedAdapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, true, servicedPlugins)
        this.audioPluginServiceListView.adapter = servicedAdapter

        val localPlugins = servicedPlugins.filter { p -> p.first.packageName == applicationInfo.packageName && p.second.backend == "LV2" }.toTypedArray()
        val localAdapter = PluginViewAdapter(this, R.layout.audio_plugin_service_list_item, false, localPlugins)
        this.localAudioPluginListView.adapter = localAdapter

        playPrePluginLabel.setOnClickListener { Thread{playSound(44100, false) }.run() }
        playPostPluginLabel.setOnClickListener { Thread{playSound(44100, true) }.run() }

        wavePrePlugin.setRawData(in_raw, {})
        wavePrePlugin.progress = 100f
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
