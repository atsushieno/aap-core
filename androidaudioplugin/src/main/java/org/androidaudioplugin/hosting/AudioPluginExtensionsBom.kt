package org.androidaudioplugin.hosting

import org.androidaudioplugin.ExtensionInformation

object AudioPluginExtensionsBom {
    const val AAP_PARAMETERS_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/parameters/v4"
    const val AAP_PRESETS_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/presets/v4"
    const val AAP_GUI_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/gui/v4"
    const val AAP_MIDI_EXTENSION_URI_V3 = "urn://androidaudioplugin.org/extensions/midi2/v3"
    const val AAP_STATE_EXTENSION_URI_V4 = "urn://androidaudioplugin.org/extensions/state/v4"
    const val AAP_BUSES_EXTENSION_URI_V1 = "urn://androidaudioplugin.org/extensions/buses/v1"

    // `<extensions bom="..." />` in aap_metadata.xml: the extensions (and their versions) of the BOM.
    // ("0.12.0" is a misnomer, but it is kept as is.)
    private val BOM_0_12_0 = listOf(
        AAP_PARAMETERS_EXTENSION_URI_V4,
        AAP_STATE_EXTENSION_URI_V4,
        AAP_PRESETS_EXTENSION_URI_V4,
        AAP_MIDI_EXTENSION_URI_V3,
        AAP_GUI_EXTENSION_URI_V4)
    private val BOM_0_12_1 = BOM_0_12_0 + AAP_BUSES_EXTENSION_URI_V1

    private val boms = mapOf(
        "0.12.0" to BOM_0_12_0,
        "0.12.1" to BOM_0_12_1,
    )

    fun fillExtensionsFromBom(extensions: MutableList<ExtensionInformation>, bom: String) {
        boms[bom]?.forEach { extensions.add(ExtensionInformation(false, it)) }
    }
}