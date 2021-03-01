package org.androidaudioplugin.ui.traditional

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ArrayAdapter
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.isVisible
import org.androidaudioplugin.AudioPluginHostHelper
import org.androidaudioplugin.PluginInformation
import org.androidaudioplugin.PortInformation
import org.androidaudioplugin.ui.traditional.databinding.AudioPluginParametersListItemBinding
import org.androidaudioplugin.ui.traditional.databinding.LocalPluginDetailsBinding
import java.lang.UnsupportedOperationException

class LocalPluginDetailsActivity : AppCompatActivity() {
    lateinit var binding : LocalPluginDetailsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = LocalPluginDetailsBinding.inflate(LayoutInflater.from(this))
        setContentView(binding.root)

        binding.wavePostPlugin.sample = arrayOf(0).toIntArray() // dummy

        val pluginId = intent.getStringExtra("pluginId")

        plugin = AudioPluginHostHelper.queryAudioPlugins(this).first { p -> p.pluginId == pluginId }

        val portsAdapter = PortViewAdapter(this, R.layout.audio_plugin_parameters_list_item, plugin.ports)
        binding.audioPluginParametersListView.adapter = portsAdapter

        binding.pluginNameLabel.text = plugin.displayName

        // FIXME: enable them once we get some apply preview functionality.
        binding.applyToggleButton.isVisible = false
        binding.playPostPluginLabel.isVisible = false
        binding.wavePostPlugin.isVisible = false

    }

    private lateinit var plugin : PluginInformation

    inner class PortViewAdapter(
        ctx: Context,
        layout: Int,
        private val ports: List<PortInformation>
    ) : ArrayAdapter<PortInformation>(ctx, layout, ports) {

        override fun getView(position: Int, convertView: View?, parent: ViewGroup): View {
            val binding = AudioPluginParametersListItemBinding.inflate(LayoutInflater.from(this@LocalPluginDetailsActivity))
            val item = getItem(position)
            val view = binding.root
            if (item == null)
                throw UnsupportedOperationException("item not available!?")

            binding.audioPluginParameterContentType.text = if (item.content == 1) "Audio" else if (item.content == 2) "Midi" else "Other"
            binding.audioPluginParameterDirection.text = if (item.direction == 0) "In" else "Out"
            binding.audioPluginParameterName.text = item.name
            binding.audioPluginParameterValue.addOnChangeListener{ _, value, _ ->
                    parameters[position] = value
                    binding.audioPluginEditTextParameterValue.text.clear()
                    binding.audioPluginEditTextParameterValue.text.insert(0, value.toString())
            }
            val value = parameters[ports.indexOf(item)]
            binding.audioPluginEditTextParameterValue.text.clear()
            binding.audioPluginEditTextParameterValue.text.insert(0, value.toString())
            binding.audioPluginParameterValue.value = value
            binding.audioPluginParameterValue.valueFrom = item.minimum
            binding.audioPluginParameterValue.valueTo = item.maximum

            return view
        }

        private val parameters = ports.map { p -> p.default }.toFloatArray()

        init {
            for (i in parameters.indices)
                parameters[i] = ports[i].default
        }
    }
}