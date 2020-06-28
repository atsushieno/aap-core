package org.androidaudioplugin.ui.androidx

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import android.widget.SeekBar
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isVisible
import kotlinx.android.synthetic.main.audio_plugin_parameters_list_item.view.*
import kotlinx.android.synthetic.main.local_plugin_details.*
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation

class LocalPluginDetailsActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.local_plugin_details)
        wavePostPlugin.sample = arrayOf(0).toIntArray() // dummy

        var pluginId = intent.getStringExtra("pluginId")

        plugin = AudioPluginHostHelper.queryAudioPlugins(this).first { p -> p.pluginId == pluginId }

        var portsAdapter = PortViewAdapter(this, R.layout.audio_plugin_parameters_list_item, plugin.ports)
        this.audioPluginParametersListView.adapter = portsAdapter

        this.pluginNameLabel.text = plugin.displayName

        // FIXME: enable them once we get some apply preview functionality.
        this.applyToggleButton.isVisible = false
        this.playPostPluginLabel.isVisible = false
        this.wavePostPlugin.isVisible = false
    }

    lateinit var plugin : PluginInformation

    inner class PortViewAdapter(ctx: Context, layout: Int, private var ports: List<PortInformation>)
        : ArrayAdapter<PortInformation>(ctx, layout, ports) {
        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            val item = getItem(position)
            var view = convertView
            if (view == null)
                view = LayoutInflater.from(context).inflate(R.layout.audio_plugin_parameters_list_item, parent, false)
            if (view == null)
                throw UnsupportedOperationException()
            if (item == null)
                return view

            view.audio_plugin_parameter_content_type.text = if (item.content == 1) "Audio" else if (item.content == 2) "Midi" else "Other"
            view.audio_plugin_parameter_direction.text = if (item.direction == 0) "In" else "Out"
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
}