package org.androidaudioplugin.hosting

import android.content.Context
import androidx.preference.PreferenceManager

class AudioPluginMidiSettings(context: Context, private val instance: AudioPluginInstance) {

    companion object {
        private const val settingsKeyBase = "midi-settings-flags"

        // keep these in sync with aap_parameters_mapping_policy in parameters extension.
        val AAP_PARAMETERS_MAPPING_POLICY_NONE = 0
        val AAP_PARAMETERS_MAPPING_POLICY_ACC = 1
        val AAP_PARAMETERS_MAPPING_POLICY_CC = 2
        val AAP_PARAMETERS_MAPPING_POLICY_SYSEX8 = 4
        val AAP_PARAMETERS_MAPPING_POLICY_PROGRAM = 8
        val AAP_PARAMETERS_MAPPING_POLICY_DEFAULT = AAP_PARAMETERS_MAPPING_POLICY_ACC or
                AAP_PARAMETERS_MAPPING_POLICY_SYSEX8 or
                AAP_PARAMETERS_MAPPING_POLICY_PROGRAM

        @JvmStatic
        fun putMidiSettingsToSharedPreference(context: Context, pluginId: String, flags: Int) {
            val manager = PreferenceManager.getDefaultSharedPreferences(context)
            manager.edit()
                .putInt("$pluginId-$settingsKeyBase", flags)
                .apply()
        }

        // It can be invoked via JNI
        @JvmStatic
        fun getMidiSettingsFromSharedPreference(context: Context, pluginId: String) =
            PreferenceManager.getDefaultSharedPreferences(context).getInt("$pluginId-$settingsKeyBase", AAP_PARAMETERS_MAPPING_POLICY_DEFAULT)
    }
}
