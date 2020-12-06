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
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.ui.androidx.databinding.AudioPluginParametersListItemBinding
import org.androidaudioplugin.ui.androidx.databinding.LocalPluginDetailsBinding
import java.lang.UnsupportedOperationException

class LocalPluginDetailsActivity : AppCompatActivity() {
    lateinit var binding : LocalPluginDetailsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = LocalPluginDetailsBinding.inflate(LayoutInflater.from(this))
        setContentView(binding.root)

        binding.wavePostPlugin.sample = arrayOf(0).toIntArray() // dummy

        var pluginId = intent.getStringExtra("pluginId")

        plugin = AudioPluginHostHelper.queryAudioPlugins(this).first { p -> p.pluginId == pluginId }

        var portsAdapter = PortViewAdapter(this, R.layout.audio_plugin_parameters_list_item, plugin.ports)
        binding.audioPluginParametersListView.adapter = portsAdapter

        binding.pluginNameLabel.text = plugin.displayName

        // FIXME: enable them once we get some apply preview functionality.
        binding.applyToggleButton.isVisible = false
        binding.playPostPluginLabel.isVisible = false
        binding.wavePostPlugin.isVisible = false
    }

    lateinit var plugin : PluginInformation

    inner class PortViewAdapter(ctx: Context, layout: Int, private var ports: List<PortInformation>)
        : ArrayAdapter<PortInformation>(ctx, layout, ports) {

        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            // lint warns about this saying I should not inflate this every time, but that results in
            // reusing binding root
            val binding =
                if (convertView != null) AudioPluginParametersListItemBinding.bind(convertView)
                else AudioPluginParametersListItemBinding.inflate(LayoutInflater.from(this@LocalPluginDetailsActivity))
            val item = getItem(position)
            var view = binding.root
            if (item == null)
                throw UnsupportedOperationException("item not available!?")

            binding.audioPluginParameterContentType.text = if (item.content == 1) "Audio" else if (item.content == 2) "Midi" else "Other"
            binding.audioPluginParameterDirection.text = if (item.direction == 0) "In" else "Out"
            binding.audioPluginParameterName.text = item.name
            binding.audioPluginSeekbarParameterValue.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                    parameters[position] = progress / 100.0f
                    binding.audioPluginEditTextParameterValue.text.clear()
                    binding.audioPluginEditTextParameterValue.text.insert(0, parameters[position].toString())
                }

                override fun onStartTrackingTouch(seekBar: SeekBar?) {
                }

                override fun onStopTrackingTouch(seekBar: SeekBar?) {
                }
            })
            var value = parameters[ports.indexOf(item)]
            binding.audioPluginSeekbarParameterValue.progress = (100.0 * value).toInt()

            return view
        }

        val parameters = ports.map { p -> p.default }.toFloatArray()
    }
}