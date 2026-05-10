package org.androidaudioplugin.hosting

import org.androidaudioplugin.ExtensionInformation

object AudioPluginExtensionsBom {
    const val AAP_PARAMETERS_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/parameters/v4"
    const val AAP_PRESETS_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/presets/v4"
    const val AAP_GUI_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/gui/v4"
    const val AAP_MIDI_EXTENSION_URI_V3 = "urn://androidaudioplugin.org/extensions/midi2/v3"
    const val AAP_STATE_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/state/v4"
    const val AAP_PORT_CONFIG_EXTENSION_URI_V3 = "urn://androidaudioplugin.org/extensions/port-config/v3"

    fun fillExtensionsFromBom(extensions: MutableList<ExtensionInformation>, bom: String) {
        when (bom) {
            "0.12.0" -> {
                extensions.add(ExtensionInformation(false, AAP_PARAMETERS_EXTENSION_URI_V4))
                extensions.add(ExtensionInformation(false, AAP_STATE_EXTENSION_URI_V4))
                extensions.add(ExtensionInformation(false, AAP_PRESETS_EXTENSION_URI_V4))
                extensions.add(ExtensionInformation(false, AAP_MIDI_EXTENSION_URI_V3))
                extensions.add(ExtensionInformation(false, AAP_PORT_CONFIG_EXTENSION_URI_V3))
                extensions.add(ExtensionInformation(false, AAP_GUI_EXTENSION_URI_V4))
            }
        }
    }
}